#include "fake_serial_port.h"
#include "em_expectations.h"
#include "milur_device.h"
#include "mercury230_device.h"
#include "mercury200_device.h"

namespace {
    PSlaveEntry MilurSlave = TSlaveEntry::Intern("milur", 0xff);
    PSlaveEntry Mercury230Slave = TSlaveEntry::Intern("mercury230", 0x00);
    PSlaveEntry Mercury200dot02Slave = TSlaveEntry::Intern("mercury200dot02", 123456);
    PRegister MilurPhaseCVoltageReg = TRegister::Intern(MilurSlave, TMilurDevice::REG_PARAM, 102, U24);
    PRegister MilurTotalConsumptionReg = TRegister::Intern(MilurSlave, TMilurDevice::REG_ENERGY, 118, BCD32);
    PRegister Mercury230TotalConsumptionReg =
        TRegister::Intern(Mercury230Slave, TMercury230Device::REG_VALUE_ARRAY, 0x0000, U32);
    PRegister Mercury230TotalReactiveEnergyReg =
        TRegister::Intern(Mercury230Slave, TMercury230Device::REG_VALUE_ARRAY, 0x0002, U32);
    PRegister Mercury230U1Reg = TRegister::Intern(Mercury230Slave, TMercury230Device::REG_PARAM, 0x1111, U24);
    PRegister Mercury230I1Reg = TRegister::Intern(Mercury230Slave, TMercury230Device::REG_PARAM, 0x1121, U24);
    PRegister Mercury230U2Reg = TRegister::Intern(Mercury230Slave, TMercury230Device::REG_PARAM, 0x1112, U24);
    PRegister Mercury230PReg = TRegister::Intern(Mercury230Slave, TMercury230Device::REG_PARAM, 0x1100, U24);
    PRegister M200dot02RET1Reg = TRegister::Intern(Mercury200dot02Slave,
                                                   TMercury20002Device::REG_ENERGY_VALUE, 0x00, BCD32);
    PRegister M200dot02RET2Reg = TRegister::Intern(Mercury200dot02Slave,
                                                   TMercury20002Device::REG_ENERGY_VALUE, 0x01, BCD32);
    PRegister M200dot02RET3Reg = TRegister::Intern(Mercury200dot02Slave,
                                                   TMercury20002Device::REG_ENERGY_VALUE, 0x02, BCD32);
    PRegister M200dot02RET4Reg = TRegister::Intern(Mercury200dot02Slave,
                                                   TMercury20002Device::REG_ENERGY_VALUE, 0x03, BCD32);
    PRegister M200dot02UReg
        = TRegister::Intern(Mercury200dot02Slave, TMercury20002Device::REG_PARAM_VALUE16, 0x00, BCD16);
    PRegister M200dot02IReg
        = TRegister::Intern(Mercury200dot02Slave, TMercury20002Device::REG_PARAM_VALUE16, 0x01, BCD16);
    PRegister M200dot02PReg
        = TRegister::Intern(Mercury200dot02Slave, TMercury20002Device::REG_PARAM_VALUE24, 0x02, BCD24);
};

class TEMDeviceTest : public TSerialDeviceTest, public TEMDeviceExpectations {
protected:
    void SetUp();
    void VerifyMilurQuery();
    void VerifyMercuryParamQuery();
    void VerifyM200dot02EnergyQuery();
    void VerifyM200dot02ParamQuery();
    virtual PDeviceConfig MilurConfig();
    virtual PDeviceConfig Mercury230Config();
    virtual PDeviceConfig M200dot02Config();
    PMilurDevice MilurDev;
    PMercury230Device Mercury230Dev;
    PMercury20002Device M200dot02Dev;
};

PDeviceConfig TEMDeviceTest::MilurConfig()
{
    return std::make_shared<TDeviceConfig>("milur", 0xff, "milur");
}

PDeviceConfig TEMDeviceTest::Mercury230Config()
{
    return std::make_shared<TDeviceConfig>("mercury230", 0x00, "mercury230");
}

PDeviceConfig TEMDeviceTest::M200dot02Config()
{
    return std::make_shared<TDeviceConfig>("mercury200dot02", 0x00112233, "mercury200dot02");
}

void TEMDeviceTest::SetUp()
{
    TSerialDeviceTest::SetUp();
    MilurDev = std::make_shared<TMilurDevice>(MilurConfig(), SerialPort);
    Mercury230Dev = std::make_shared<TMercury230Device>(Mercury230Config(), SerialPort);
    M200dot02Dev = std::make_shared<TMercury20002Device>(M200dot02Config(), SerialPort);
    SerialPort->Open();
}

void TEMDeviceTest::VerifyMilurQuery()
{
    EnqueueMilurPhaseCVoltageResponse();
    ASSERT_EQ(0x03946f, MilurDev->ReadRegister(MilurPhaseCVoltageReg));

    EnqueueMilurTotalConsumptionResponse();
    // "milur BCD32" value 11144 packed as uint64_t
    ASSERT_EQ(4904702568694808576, MilurDev->ReadRegister(MilurTotalConsumptionReg));
}

TEST_F(TEMDeviceTest, MilurQuery)
{
    EnqueueMilurSessionSetupResponse();
    VerifyMilurQuery();
    VerifyMilurQuery();
    SerialPort->Close();
}

TEST_F(TEMDeviceTest, MilurReconnect)
{
    EnqueueMilurSessionSetupResponse();
    EnqueueMilurNoSessionResponse();
    // reconnection
    EnqueueMilurSessionSetupResponse();
    EnqueueMilurPhaseCVoltageResponse();
    ASSERT_EQ(0x03946f, MilurDev->ReadRegister(MilurPhaseCVoltageReg));
}

TEST_F(TEMDeviceTest, MilurException)
{
    EnqueueMilurSessionSetupResponse();
    EnqueueMilurExceptionResponse();
    try {
        MilurDev->ReadRegister(MilurPhaseCVoltageReg);
        FAIL() << "No exception thrown";
    } catch (const TSerialDeviceException& e) {
        ASSERT_STREQ("Serial protocol error: EEPROM access error", e.what());
        SerialPort->Close();
    }
}

TEST_F(TEMDeviceTest, Mercury230ReadEnergy)
{
    EnqueueMercury230SessionSetupResponse();
    EnqueueMercury230EnergyResponse1();

    // Register address for energy arrays:
    // 0000 0000 CCCC CCCC TTTT AAAA MMMM IIII
    // C = command (0x05)
    // A = array number
    // M = month
    // T = tariff (FIXME!!! 5 values)
    // I = index
    // Note: for A=6, 12-byte and not 16-byte value is returned.
    // This is not supported at the moment.

    // Here we make sure that consecutive requests querying the same array
    // don't cause redundant requests during the single poll cycle.
    ASSERT_EQ(3196200, Mercury230Dev->ReadRegister(Mercury230TotalConsumptionReg));
    ASSERT_EQ(300444, Mercury230Dev->ReadRegister(Mercury230TotalReactiveEnergyReg));
    ASSERT_EQ(3196200, Mercury230Dev->ReadRegister(Mercury230TotalConsumptionReg));
    Mercury230Dev->EndPollCycle();

    EnqueueMercury230EnergyResponse2();
    ASSERT_EQ(3196201, Mercury230Dev->ReadRegister(Mercury230TotalConsumptionReg));
    ASSERT_EQ(300445, Mercury230Dev->ReadRegister(Mercury230TotalReactiveEnergyReg));
    ASSERT_EQ(3196201, Mercury230Dev->ReadRegister(Mercury230TotalConsumptionReg));
    Mercury230Dev->EndPollCycle();
    SerialPort->Close();
}

void TEMDeviceTest::VerifyMercuryParamQuery()
{
    EnqueueMercury230U1Response();
    // Register address for params:
    // 0000 0000 CCCC CCCC NNNN NNNN BBBB BBBB
    // C = command (0x08)
    // N = param number (0x11)
    // B = subparam spec (BWRI), 0x11 = voltage, phase 1
    ASSERT_EQ(24128, Mercury230Dev->ReadRegister(Mercury230U1Reg));

    EnqueueMercury230I1Response();
    // subparam 0x21 = current (phase 1)
    ASSERT_EQ(69, Mercury230Dev->ReadRegister(Mercury230I1Reg));

    EnqueueMercury230U2Response();
    // subparam 0x12 = voltage (phase 2)
    ASSERT_EQ(24043, Mercury230Dev->ReadRegister(Mercury230U2Reg));

    EnqueueMercury230PResponse();
    // Total power (P)
    ASSERT_EQ(553095, Mercury230Dev->ReadRegister(Mercury230PReg));
}

void TEMDeviceTest::VerifyM200dot02EnergyQuery()
{
    EnqueueM200dot02EnergyResponse();
    // BCD32 0x00062142
    ASSERT_EQ(1109460480, M200dot02Dev->ReadRegister(M200dot02RET1Reg));
    // BCD32 0x00020834
    ASSERT_EQ(872940032, M200dot02Dev->ReadRegister(M200dot02RET2Reg));
    // BCD32 0x00011111
    ASSERT_EQ(286327040, M200dot02Dev->ReadRegister(M200dot02RET3Reg));
    // BCD32 0x00022222
    ASSERT_EQ(572654080, M200dot02Dev->ReadRegister(M200dot02RET4Reg));
    M200dot02Dev->EndPollCycle();
//    SerialPort->Close();
}

void TEMDeviceTest::VerifyM200dot02ParamQuery()
{
    EnqueueM200dot02ParamResponse();
    // BCB16 0x00001234
    ASSERT_EQ(873594880, M200dot02Dev->ReadRegister(M200dot02UReg));
    // BCB16 0x00005678
    ASSERT_EQ(2018902016, M200dot02Dev->ReadRegister(M200dot02IReg));
    // BCB24 0x00765432
    ASSERT_EQ(844396032, M200dot02Dev->ReadRegister(M200dot02PReg));
    M200dot02Dev->EndPollCycle();
//    SerialPort->Close();
}

TEST_F(TEMDeviceTest, Mercury230ReadParams)
{
    EnqueueMercury230SessionSetupResponse();
    VerifyMercuryParamQuery();
    Mercury230Dev->EndPollCycle();
    VerifyMercuryParamQuery();
    Mercury230Dev->EndPollCycle();
    SerialPort->Close();
}

TEST_F(TEMDeviceTest, Mercury230Reconnect)
{
    EnqueueMercury230SessionSetupResponse();
    EnqueueMercury230NoSessionResponse();
    // re-setup happens here
    EnqueueMercury230SessionSetupResponse();
    EnqueueMercury230U2Response();

    // subparam 0x12 = voltage (phase 2)
    ASSERT_EQ(24043, Mercury230Dev->ReadRegister(Mercury230U2Reg));

    Mercury230Dev->EndPollCycle();
    SerialPort->Close();
}

TEST_F(TEMDeviceTest, Mercury230Exception)
{
    EnqueueMercury230SessionSetupResponse();
    EnqueueMercury230InternalMeterErrorResponse();
    try {
        Mercury230Dev->ReadRegister(Mercury230U2Reg);
        FAIL() << "No exception thrown";
    } catch (const TSerialDeviceException& e) {
        ASSERT_STREQ("Serial protocol error: Internal meter error", e.what());
        SerialPort->Close();
    }
}

TEST_F(TEMDeviceTest, Mercury200dot02Energy)
{
    try {
        VerifyM200dot02EnergyQuery();
    } catch(const TSerialDeviceException& e) {
        SerialPort->Close();
        FAIL() <<  e.what();
    } catch(const std::exception& e) {
        SerialPort->Close();
        FAIL() << e.what();
    }
    SerialPort->Close();
}

TEST_F(TEMDeviceTest, Mercury200dot02Params)
{
    try {
        VerifyM200dot02ParamQuery();
    } catch(const TSerialDeviceException& e) {
        SerialPort->Close();
        FAIL() <<  e.what();
    } catch(const std::exception& e) {
        SerialPort->Close();
        FAIL() << e.what();
    }
    SerialPort->Close();
}

TEST_F(TEMDeviceTest, Combined)
{
    EnqueueMilurSessionSetupResponse();
    VerifyMilurQuery();
    MilurDev->EndPollCycle();

    EnqueueMercury230SessionSetupResponse();
    VerifyMercuryParamQuery();
    Mercury230Dev->EndPollCycle();

    for (int i = 0; i < 3; i++) {
        VerifyMilurQuery();
        MilurDev->EndPollCycle();

        VerifyMercuryParamQuery();
        Mercury230Dev->EndPollCycle();
    }

    SerialPort->Close();
}

class TEMCustomPasswordTest : public TEMDeviceTest {
public:
    PDeviceConfig MilurConfig();

    PDeviceConfig Mercury230Config();
};

PDeviceConfig TEMCustomPasswordTest::MilurConfig()
{
    PDeviceConfig device_config = TEMDeviceTest::MilurConfig();
    device_config->Password = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    device_config->AccessLevel = 2;
    return device_config;
}

PDeviceConfig TEMCustomPasswordTest::Mercury230Config()
{
    PDeviceConfig device_config = TEMDeviceTest::Mercury230Config();
    device_config->Password = {0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    device_config->AccessLevel = 2;
    return device_config;
}

TEST_F(TEMCustomPasswordTest, Combined)
{
    EnqueueMilurAccessLevel2SessionSetupResponse();
    VerifyMilurQuery();
    MilurDev->EndPollCycle();

    EnqueueMercury230AccessLevel2SessionSetupResponse();
    VerifyMercuryParamQuery();
    Mercury230Dev->EndPollCycle();

    SerialPort->Close();
}
