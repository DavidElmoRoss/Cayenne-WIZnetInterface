/**
* Example app for using the Cayenne MQTT C++ library to send and receive example data. This example uses
* the WIZnetInterface library to connect via Ethernet.
*/

#include "MQTTTimer.h"
#include "CayenneMQTTClient.h"
#include "MQTTNetwork.h"
#include "EthernetInterface.h"
	
// Cayenne authentication info. This should be obtained from the Cayenne Dashboard.
char* username = "MQTT_USERNAME";
char* password = "MQTT_PASSWORD";
char* clientID = "CLIENT_ID";

SPI spi(D11, D12, D13);
EthernetInterface interface(&spi, D10, D5); // SPI, SEL, Reset
MQTTNetwork<EthernetInterface> network(interface);
CayenneMQTT::MQTTClient<MQTTNetwork<EthernetInterface>, MQTTTimer> mqttClient(network, username, password, clientID);

/**
* Print the message info.
* @param[in] message The message received from the Cayenne server.
*/
void outputMessage(CayenneMQTT::MessageData& message)
{
    switch (message.topic)  {
    case COMMAND_TOPIC:
        printf("topic=Command");
        break;
    case CONFIG_TOPIC:
        printf("topic=Config");
        break;
    default:
        printf("topic=%d", message.topic);
        break;
    }
    printf(" channel=%d", message.channel);
    if (message.clientID) {
        printf(" clientID=%s", message.clientID);
    }
    if (message.type) {
        printf(" type=%s", message.type);
    }
    for (size_t i = 0; i < message.valueCount; ++i) {
		if (message.getValue(i)) {
			printf(" value=%s", message.getValue(i));
		}
		if (message.getUnit(i)) {
			printf(" unit=%s", message.getUnit(i));
		}
    }
    if (message.id) {
        printf(" id=%s", message.id);
    }
    printf("\n");
}

/**
* Handle messages received from the Cayenne server.
* @param[in] message The message received from the Cayenne server.
*/
void messageArrived(CayenneMQTT::MessageData& message)
{
    int error = 0;
    // Add code to process the message. Here we just ouput the message data.
    outputMessage(message);

	if (message.topic == COMMAND_TOPIC) {
		// If this is a command message we publish a response to show we recieved it. Here we are just sending a default 'OK' response.
		// An error response should be sent if there are issues processing the message.
		if ((error = mqttClient.publishResponse(message.channel, message.id, NULL, message.clientID)) != CAYENNE_SUCCESS) {
			printf("Response failure, error: %d\n", error);
		}
			
		// Send the updated state for the channel so it is reflected in the Cayenne dashboard. If a command is successfully processed
		// the updated state will usually just be the value received in the command message.
		if ((error = mqttClient.publishData(DATA_TOPIC, message.channel, NULL, NULL, message.getValue())) != CAYENNE_SUCCESS) {
			printf("Publish state failure, error: %d\n", error);
		}
	}
}

/**
* Connect to the Cayenne server.
* @return Returns CAYENNE_SUCCESS if the connection succeeds, or an error code otherwise.
*/
int connectClient(void)
{
    int error = 0;
    // Connect to the server.
    printf("Connecting to %s:%d\n", CAYENNE_DOMAIN, CAYENNE_PORT);
    while ((error = network.connect(CAYENNE_DOMAIN, CAYENNE_PORT)) != 0) {
        printf("TCP connect failed, error: %d\n", error);
        wait(2);
    }

    if ((error = mqttClient.connect()) != MQTT::SUCCESS) {
        printf("MQTT connect failed, error: %d\n", error);
        return error;
    }
    printf("Connected\n");

    // Subscribe to required topics.
    if ((error = mqttClient.subscribe(COMMAND_TOPIC, CAYENNE_ALL_CHANNELS)) != CAYENNE_SUCCESS) {
        printf("Subscription to Command topic failed, error: %d\n", error);
    }
    if ((error = mqttClient.subscribe(CONFIG_TOPIC, CAYENNE_ALL_CHANNELS)) != CAYENNE_SUCCESS) {
        printf("Subscription to Config topic failed, error:%d\n", error);
    }

    // Send device info. Here we just send some example values for the system info. These should be changed to use actual system data, or removed if not needed.
    mqttClient.publishData(SYS_VERSION_TOPIC, CAYENNE_NO_CHANNEL, NULL, NULL, CAYENNE_VERSION);
    mqttClient.publishData(SYS_MODEL_TOPIC, CAYENNE_NO_CHANNEL, NULL, NULL, "mbedDevice");
    //mqttClient.publishData(SYS_CPU_MODEL_TOPIC, CAYENNE_NO_CHANNEL, NULL, NULL, "CPU Model");
    //mqttClient.publishData(SYS_CPU_SPEED_TOPIC, CAYENNE_NO_CHANNEL, NULL, NULL, "1000000000");

    return CAYENNE_SUCCESS;
}

/**
* Main loop where MQTT code is run.
*/
void loop(void)
{
    MQTTTimer timer(5000);

    while (true) {
        // Yield to allow MQTT message processing.
        mqttClient.yield(1000);

        // Check that we are still connected, if not, reconnect.
        if (!network.connected() || !mqttClient.connected()) {
            network.disconnect();
            mqttClient.disconnect();
            printf("Reconnecting\n");
            while (connectClient() != CAYENNE_SUCCESS) {
                wait(2);
                printf("Reconnect failed, retrying\n");
            }
        }

        // Publish some example data every few seconds. This should be changed to send your actual data to Cayenne.
        if (timer.expired()) {
            int error = 0;
            if ((error = mqttClient.publishData(DATA_TOPIC, 0, TEMPERATURE, CELSIUS, 30.5)) != CAYENNE_SUCCESS) {
                printf("Publish temperature failed, error: %d\n", error);
            }
            if ((error = mqttClient.publishData(DATA_TOPIC, 1, LUMINOSITY, LUX, 1000)) != CAYENNE_SUCCESS) {
                printf("Publish luminosity failed, error: %d\n", error);
            }
            if ((error = mqttClient.publishData(DATA_TOPIC, 2, BAROMETRIC_PRESSURE, HECTOPASCAL, 800)) != CAYENNE_SUCCESS) {
                printf("Publish barometric pressure failed, error: %d\n", error);
            }
            timer.countdown_ms(5000);
        }
    }
}

/**
* Main function.
*/
int main()
{
    printf("Initializing interface\n");
    // Set the correct SPI frequency for your shield, if necessary. For example, 42000000 for Arduino Ethernet Shield W5500 or 20000000 for Arduino Ethernet Shield W5100.
    spi.frequency(42000000);
	unsigned char MAC_Addr[6] = {0xFE,0x08,0xDC,0x12,0x34,0x56};
    mbed_mac_address((char *)MAC_Addr); // Use mbed mac address
    interface.init(MAC_Addr);   

    // Set the default function that receives Cayenne messages.
    mqttClient.setDefaultMessageHandler(messageArrived);

    // Connect to Cayenne.
    if (connectClient() == CAYENNE_SUCCESS) {
        // Run main loop.
        loop();
    }
    else {
        printf("Connection failed, exiting\n");
    }

    if (mqttClient.connected())
        mqttClient.disconnect();
    if (network.connected())
        network.disconnect();

    return 0;
}

