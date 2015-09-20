#pragma once
#include "imx233.h"

#define	LRADC_CTRL2_TEMP_SENSOR_IENABLE1	(1 << 9)
#define	LRADC_CTRL2_TEMP_SENSOR_IENABLE0	(1 << 8)
#define	LRADC_CTRL2_TEMP_ISRC1_OFFSET		4
#define	LRADC_CTRL2_TEMP_ISRC0_OFFSET		0
#define	LRADC_CTRL2_TEMP_ISRC_MASK		(0x0f)

// "channel" here means current source channel
// imx233: ichan 0 == lradc0, ichan 1 == lradc1
// imx28:  ichan 0 == lradc0, ichan 1 == lradc6

void SetUpCurrentSource(int channel, unsigned current_ua);
void SwitchOffCurrentSource(int channel);

int GetCurrentSourceChannelNumber(int lradc_channel);
