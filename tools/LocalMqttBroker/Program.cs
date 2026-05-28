using System.Buffers;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using MQTTnet;
using MQTTnet.Protocol;
using MQTTnet.Server;

var options = BrokerOptions.Parse(args);
var brokerAddress = GetPreferredIPv4Address(options.InterfaceAlias);

Console.WriteLine("IoTEmbedded local MQTT broker");
Console.WriteLine($"Listen: 0.0.0.0:{options.Port}");
Console.WriteLine($"Board broker IP: {brokerAddress}");
Console.WriteLine("Topics: /v1/devices/up/# and any other MQTT publish");
Console.WriteLine("Press Ctrl+C to stop.");
Console.WriteLine();

var factory = new MqttServerFactory();
var mqttServerOptions = factory.CreateServerOptionsBuilder()
    .WithDefaultEndpoint()
    .WithDefaultEndpointBoundIPAddress(IPAddress.Any)
    .WithDefaultEndpointPort(options.Port)
    .Build();

var mqttServer = factory.CreateMqttServer(mqttServerOptions);

mqttServer.ValidatingConnectionAsync += context =>
{
    var keepAlive = context.KeepAlivePeriod.HasValue ? $"{context.KeepAlivePeriod.Value}s" : "-";
    WriteLine("CONNECT",
        $"client={context.ClientId} user={context.UserName ?? "-"} keepAlive={keepAlive}");
    return Task.CompletedTask;
};

mqttServer.ClientConnectedAsync += context =>
{
    WriteLine("CONNECTED",
        $"client={context.ClientId} user={context.UserName ?? "-"} remote={context.RemoteEndPoint} protocol={context.ProtocolVersion}");
    return Task.CompletedTask;
};

mqttServer.ClientDisconnectedAsync += context =>
{
    WriteLine("DISCONNECTED", $"client={context.ClientId} reason={context.ReasonCode}");
    return Task.CompletedTask;
};

mqttServer.ClientSubscribedTopicAsync += context =>
{
    WriteLine("SUBSCRIBE", $"client={context.ClientId} topic={context.TopicFilter.Topic} qos={context.TopicFilter.QualityOfServiceLevel}");
    return Task.CompletedTask;
};

mqttServer.InterceptingPublishAsync += context =>
{
    var message = context.ApplicationMessage;
    var payload = DecodePayload(message.Payload);
    var qos = ToQosNumber(message.QualityOfServiceLevel);
    WriteLine("PUBLISH",
        $"client={context.ClientId} user={context.UserName ?? "-"} topic={message.Topic} qos={qos} retain={message.Retain} bytes={Encoding.UTF8.GetByteCount(payload)}");

    if (!string.IsNullOrWhiteSpace(payload))
    {
        Console.WriteLine(FormatPayload(payload));
    }

    Console.WriteLine();
    return Task.CompletedTask;
};

using var stopping = new CancellationTokenSource();
Console.CancelKeyPress += (_, eventArgs) =>
{
    eventArgs.Cancel = true;
    stopping.Cancel();
};

await mqttServer.StartAsync();

if (options.SelfTest)
{
    await PublishSelfTestAsync(options.Port);
}

try
{
    await Task.Delay(Timeout.InfiniteTimeSpan, stopping.Token);
}
catch (OperationCanceledException)
{
}

await mqttServer.StopAsync(new MqttServerStopOptions());

static string DecodePayload(ReadOnlySequence<byte> payload)
{
    if (payload.IsEmpty)
    {
        return string.Empty;
    }

    if (payload.IsSingleSegment)
    {
        return Encoding.UTF8.GetString(payload.FirstSpan);
    }

    return Encoding.UTF8.GetString(payload.ToArray());
}

static string FormatPayload(string payload)
{
    try
    {
        using var document = JsonDocument.Parse(payload);
        return JsonSerializer.Serialize(document.RootElement, new JsonSerializerOptions { WriteIndented = true });
    }
    catch (JsonException)
    {
        return payload;
    }
}

static int ToQosNumber(MqttQualityOfServiceLevel qos)
{
    return qos switch
    {
        MqttQualityOfServiceLevel.AtMostOnce => 0,
        MqttQualityOfServiceLevel.AtLeastOnce => 1,
        MqttQualityOfServiceLevel.ExactlyOnce => 2,
        _ => -1
    };
}

static void WriteLine(string eventName, string message)
{
    Console.WriteLine($"[{DateTimeOffset.Now:yyyy-MM-dd HH:mm:ss.fff zzz}] {eventName,-12} {message}");
}

static IPAddress GetPreferredIPv4Address(string? interfaceAlias)
{
    var interfaces = NetworkInterface.GetAllNetworkInterfaces()
        .Where(networkInterface =>
            networkInterface.OperationalStatus == OperationalStatus.Up &&
            networkInterface.NetworkInterfaceType != NetworkInterfaceType.Loopback)
        .ToArray();

    if (!string.IsNullOrWhiteSpace(interfaceAlias))
    {
        var match = interfaces.FirstOrDefault(networkInterface =>
            string.Equals(networkInterface.Name, interfaceAlias, StringComparison.OrdinalIgnoreCase) ||
            string.Equals(networkInterface.Description, interfaceAlias, StringComparison.OrdinalIgnoreCase));
        var address = GetIPv4Address(match);
        if (address is not null)
        {
            return address;
        }
    }

    var wlan = interfaces.FirstOrDefault(networkInterface =>
        networkInterface.NetworkInterfaceType == NetworkInterfaceType.Wireless80211 ||
        networkInterface.Name.Contains("WLAN", StringComparison.OrdinalIgnoreCase) ||
        networkInterface.Name.Contains("Wi-Fi", StringComparison.OrdinalIgnoreCase));
    return GetIPv4Address(wlan) ?? GetIPv4Address(interfaces.FirstOrDefault()) ?? IPAddress.Loopback;
}

static IPAddress? GetIPv4Address(NetworkInterface? networkInterface)
{
    return networkInterface?.GetIPProperties()
        .UnicastAddresses
        .Where(address => address.Address.AddressFamily == AddressFamily.InterNetwork)
        .Select(address => address.Address)
        .FirstOrDefault(address => !IPAddress.IsLoopback(address));
}

static async Task PublishSelfTestAsync(int port)
{
    var client = new MqttClientFactory().CreateMqttClient();
    var clientOptions = new MqttClientOptionsBuilder()
        .WithTcpServer(IPAddress.Loopback.ToString(), port)
        .WithClientId("local-mqtt-broker-self-test")
        .WithCleanSession()
        .Build();

    await client.ConnectAsync(clientOptions, CancellationToken.None);

    var payload = """
    {
      "type": "datas",
      "collectorId": "self-test",
      "devices": [
        {
          "type": "temperatureHumidity",
          "serviceId": 130,
          "addr": 1,
          "online": true,
          "temperature": 24.6,
          "humidity": 41.2,
          "baud": 9600
        }
      ]
    }
    """;

    var message = new MqttApplicationMessageBuilder()
        .WithTopic("/v1/devices/up/datas/self-test")
        .WithPayload(payload)
        .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtMostOnce)
        .Build();

    await client.PublishAsync(message, CancellationToken.None);
    await client.DisconnectAsync();
}

internal sealed record BrokerOptions(int Port, string? InterfaceAlias, bool SelfTest)
{
    const int DefaultPort = 1883;

    public static BrokerOptions Parse(string[] args)
    {
        var port = DefaultPort;
        string? interfaceAlias = "WLAN";
        var selfTest = false;

        for (var i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--port" when i + 1 < args.Length && int.TryParse(args[i + 1], out var parsedPort):
                    port = parsedPort;
                    i++;
                    break;
                case "--interface" when i + 1 < args.Length:
                    interfaceAlias = args[i + 1];
                    i++;
                    break;
                case "--self-test":
                    selfTest = true;
                    break;
            }
        }

        return new BrokerOptions(port, interfaceAlias, selfTest);
    }
}
