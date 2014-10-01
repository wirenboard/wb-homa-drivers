#include <map>
#include <memory>
#include <algorithm>
#include <cassert>
#include <gtest/gtest.h>

#include "testlog.h"
#include "fake_modbus.h"

class ModbusClientTest: public TLoggedFixture
{ 
protected:
    void SetUp();
    void TearDown();
    PFakeModbusConnector Connector;
    PModbusClient ModbusClient;
};

void ModbusClientTest::SetUp()
{
    TModbusConnectionSettings settings("/dev/ttyWhatever", 115200, 'N', 8, 1);
    Connector = PFakeModbusConnector(new TFakeModbusConnector(settings, *this));
    ModbusClient = PModbusClient(new TModbusClient(settings, Connector));
    ModbusClient->SetCallback([this](const TModbusRegister& reg) {
            Emit() << "Modbus Callback: " << reg.ToString() << " becomes " << 
                ModbusClient->GetTextValue(reg);
        });
}

void ModbusClientTest::TearDown()
{
    ModbusClient.reset();
    Connector.reset();
    TLoggedFixture::TearDown();
}

TEST_F(ModbusClientTest, Poll)
{
    ModbusClient->AddRegister(TModbusRegister(1, TModbusRegister::COIL, 0));
    ModbusClient->AddRegister(TModbusRegister(1, TModbusRegister::COIL, 1));
    ModbusClient->AddRegister(TModbusRegister(1, TModbusRegister::DISCRETE_INPUT, 10));
    ModbusClient->AddRegister(TModbusRegister(1, TModbusRegister::HOLDING_REGISTER, 22));
    ModbusClient->AddRegister(TModbusRegister(1, TModbusRegister::INPUT_REGISTER, 33));
    PFakeSlave slave = Connector->AddSlave(1,
                                           TRegisterRange(0, 10),
                                           TRegisterRange(10, 20),
                                           TRegisterRange(20, 30),
                                           TRegisterRange(30, 40));

    ModbusClient->Connect();

    Note() << "Cycle()";
    ModbusClient->Cycle();

    slave->Coils[1] = 1;
    slave->Discrete[10] = 1;
    slave->Holding[22] = 4242;
    slave->Input[33] = 42000;

    Note() << "Cycle()";
    ModbusClient->Cycle();

    EXPECT_EQ(0, ModbusClient->GetRawValue(
        TModbusRegister(1, TModbusRegister::COIL, 0)));
    EXPECT_EQ(1, ModbusClient->GetRawValue(
        TModbusRegister(1, TModbusRegister::COIL, 1)));
    EXPECT_EQ(1, ModbusClient->GetRawValue(
        TModbusRegister(1, TModbusRegister::DISCRETE_INPUT, 10)));
    EXPECT_EQ(4242, ModbusClient->GetRawValue(
        TModbusRegister(1, TModbusRegister::HOLDING_REGISTER, 22)));
    EXPECT_EQ(42000, ModbusClient->GetRawValue(
        TModbusRegister(1, TModbusRegister::INPUT_REGISTER, 33)));
}
