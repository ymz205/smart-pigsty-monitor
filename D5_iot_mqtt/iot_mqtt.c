/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - clarifications and/or documentation extension
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifi_connect.h"
#include "MQTTClient.h"

#include "E53_IA1.h"

#define TASK_STACK_SIZE 1024 * 8
#define TASK_PRIO 25

E53_IA1_Data_TypeDef E53_IA1_Data;

static unsigned char sendBuf[1000];
static unsigned char readBuf[1000];

Network network;

static void Example_Task(void)
{
	E53_IA1_Init();

	while (1)
	{
		// printf("\r\n=======================================\r\n");
		// printf("\r\n*************E53_IA1_example***********\r\n");
		// printf("\r\n=======================================\r\n");

		E53_IA1_Read_Data();

		// printf("\r\n******************************Lux Value is  %.2f\r\n", E53_IA1_Data.Lux);
		// printf("\r\n******************************Humidity is  %.2f\r\n", E53_IA1_Data.Humidity);
		// printf("\r\n******************************Temperature is  %.2f\r\n", E53_IA1_Data.Temperature);

		if (E53_IA1_Data.Lux < 20)
		{
			Light_StatusSet(ON);
		}
		else
		{
			Light_StatusSet(OFF);
		}

		if ((E53_IA1_Data.Humidity > 60) | (E53_IA1_Data.Temperature > 30))
		{
			Motor_StatusSet(ON);
		}
		else
		{
			Motor_StatusSet(OFF);
		}

		usleep(1000000);
	}
}

void messageArrived(MessageData *data)
{
	printf("Message arrived on topic %.*s: %.*s\n", data->topicName->lenstring.len, data->topicName->lenstring.data,
				 data->message->payloadlen, data->message->payload);
}

/* */

static void MQTT_DemoTask(void)
{
	WifiConnect("OPPOA96", "12345678");
	printf("Starting ...\n");
	int rc, count = 0;
	MQTTClient client;

	NetworkInit(&network);
	printf("NetworkConnect  ...\n");
begin:
	NetworkConnect(&network, "192.168.182.101", 1883);
	printf("MQTTClientInit  ...\n");
	MQTTClientInit(&client, &network, 2000, sendBuf, sizeof(sendBuf), readBuf, sizeof(readBuf));

	MQTTString clientId = MQTTString_initializer;
	clientId.cstring = "bearpi";

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	data.clientID = clientId;
	data.willFlag = 0;
	data.MQTTVersion = 3;
	data.keepAliveInterval = 60;
	data.cleansession = 1;

	printf("MQTTConnect  ...\n");
	rc = MQTTConnect(&client, &data);
	if (rc != 0)
	{
		printf("MQTTConnect: %d\n", rc);
		NetworkDisconnect(&network);
		MQTTDisconnect(&client);
		osDelay(200);
		goto begin;
	}

	printf("MQTTSubscribe  ...\n");
	rc = MQTTSubscribe(&client, "sensor/data", 2, messageArrived);
	if (rc != 0)
	{
		printf("MQTTSubscribe: %d\n", rc);
		osDelay(200);
		goto begin;
	}
	// 修改 MQTT_DemoTask 函数中的发布部分
	while (++count)
	{
		MQTTMessage message;
		char payload[100]; // 增大缓冲区以容纳JSON

		// 构建JSON格式数据
		sprintf(payload, "{\"Temp\":\"%.2f\",\"Humi\":\"%.2f\",\"Lux\":\"%.2f\"}",
						E53_IA1_Data.Temperature,
						E53_IA1_Data.Humidity,
						E53_IA1_Data.Lux);

		printf("Publishing JSON: %s\r\n", payload);

		message.qos = 1; // 改用 QoS 1 确保消息可靠传输
		message.retained = 0;
		message.payload = payload;
		message.payloadlen = strlen(payload);

		if ((rc = MQTTPublish(&client, "sensor/data", &message)) != 0) // 改为更清晰的topic
		{
			printf("Return code from MQTT publish is %d\n", rc);
			NetworkDisconnect(&network);
			MQTTDisconnect(&client);
			osDelay(2000);
			goto begin;
		}
		osDelay(2000); // 改为2秒发送一次
	}
}
static void MQTT_Demo(void)
{
	// 创建 MQTT 任务
	osThreadAttr_t mqttAttr;
	mqttAttr.name = "MQTT_DemoTask";
	mqttAttr.attr_bits = 0U;
	mqttAttr.cb_mem = NULL;
	mqttAttr.cb_size = 0U;
	mqttAttr.stack_mem = NULL;
	mqttAttr.stack_size = 10240;
	mqttAttr.priority = osPriorityNormal;

	if (osThreadNew((osThreadFunc_t)MQTT_DemoTask, NULL, &mqttAttr) == NULL)
	{
		printf("[MQTT_Demo] Failed to create MQTT_DemoTask!\n");
	}

	// 创建传感器任务
	osThreadAttr_t sensorAttr;
	sensorAttr.name = "Example_Task";
	sensorAttr.attr_bits = 0U;
	sensorAttr.cb_mem = NULL;
	sensorAttr.cb_size = 0U;
	sensorAttr.stack_mem = NULL;
	sensorAttr.stack_size = TASK_STACK_SIZE;
	sensorAttr.priority = TASK_PRIO;

	if (osThreadNew((osThreadFunc_t)Example_Task, NULL, &sensorAttr) == NULL)
	{
		printf("Failed to create Example_Task!\n");
	}
}

APP_FEATURE_INIT(MQTT_Demo);