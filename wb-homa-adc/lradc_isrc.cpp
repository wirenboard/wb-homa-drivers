#include <fstream>
#include <iostream>

#include "lradc_isrc.h"
#include "sysfs_adc.h"

namespace {
        extern "C" int imx233_rd(long offset);
        extern "C" int imx233_wr(long offset, long value);
};

enum class ESOCId {SOC_IMX23, SOC_IMX28, SOC_UNKNOWN };



ESOCId GetSocId()
{
	std::ifstream socFd;
    std::string socName;

	socFd.open("/sys/devices/soc0/soc_id");
	
    if (!socFd.is_open()) {
        throw TAdcException("error opening soc id file");
    }
    socFd >> socName;
    socFd.close();
    
    if (socName == "i.MX23") {
		return ESOCId::SOC_IMX23;
	} else if (socName == "i.MX28") {
		return ESOCId::SOC_IMX28;
	}    

	
	return ESOCId::SOC_UNKNOWN;
}

void SetUpCurrentSource(int channel, unsigned current_ua)
{
	if (current_ua > 300) return;
    int isrc_val = current_ua / 20;

	if (channel == 0) {
		imx233_wr(HW_LRADC_CTRL2_CLR, LRADC_CTRL2_TEMP_ISRC_MASK
				<< LRADC_CTRL2_TEMP_ISRC0_OFFSET); //clear TEMP_ISRC0

	    imx233_wr(HW_LRADC_CTRL2_SET, isrc_val << LRADC_CTRL2_TEMP_ISRC0_OFFSET
					| LRADC_CTRL2_TEMP_SENSOR_IENABLE0);
	} else if (channel == 1) {
		imx233_wr(HW_LRADC_CTRL2_CLR, LRADC_CTRL2_TEMP_ISRC_MASK
				<< LRADC_CTRL2_TEMP_ISRC1_OFFSET); //clear TEMP_ISRC1

	    imx233_wr(HW_LRADC_CTRL2_SET, isrc_val << LRADC_CTRL2_TEMP_ISRC1_OFFSET
					| LRADC_CTRL2_TEMP_SENSOR_IENABLE1);
	}
}


void SwitchOffCurrentSource(int channel)
{
	if (channel == 0) {
		imx233_wr(HW_LRADC_CTRL2_CLR, LRADC_CTRL2_TEMP_SENSOR_IENABLE0); //set TEMP_SENSOR_IENABLE0=0
	} else if (channel == 1) {
		imx233_wr(HW_LRADC_CTRL2_CLR, LRADC_CTRL2_TEMP_SENSOR_IENABLE1); //set TEMP_SENSOR_IENABLE1=0
	}
}


int GetCurrentSourceChannelNumber(int lradc_channel)
{
	auto socId = GetSocId();
	if (socId == ESOCId::SOC_IMX23) {
		if ((lradc_channel == 0) || (lradc_channel == 1)) {
			return lradc_channel;
		}
	} else if (socId == ESOCId::SOC_IMX28) {
		if (lradc_channel == 0) {
			return 0;
		} else if (lradc_channel == 6) {
			return 1;
		}
	}
	
	return -1;	
}

