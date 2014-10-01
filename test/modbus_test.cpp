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
    ModbusClient->AddRegister(TModbusRegister(1, TModbusRegister::COIL, 1));
}

void ModbusClientTest::TearDown()
{
    ModbusClient.reset();
    Connector.reset();
    TLoggedFixture::TearDown();
}

TEST_F(ModbusClientTest, Poll)
{
    PFakeSlave slave = Connector->AddSlave(1, TRegisterRange(0, 10));

    ModbusClient->Connect();
    ModbusClient->Cycle();

    slave->Coils[1] = 1;
    ModbusClient->Cycle();
    EXPECT_EQ(1, ModbusClient->GetRawValue(TModbusRegister(1, TModbusRegister::COIL, 1)));
}
