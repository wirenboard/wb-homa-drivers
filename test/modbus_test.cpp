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
    PFakeSlave Slave;
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
    Slave = Connector->AddSlave(1,
                                TRegisterRange(0, 10),
                                TRegisterRange(10, 20),
                                TRegisterRange(20, 30),
                                TRegisterRange(30, 40));
}

void ModbusClientTest::TearDown()
{
    ModbusClient.reset();
    Connector.reset();
    TLoggedFixture::TearDown();
}

TEST_F(ModbusClientTest, Poll)
{
    TModbusRegister coil0(1, TModbusRegister::COIL, 0);
    TModbusRegister coil1(1, TModbusRegister::COIL, 1);
    TModbusRegister discrete10(1, TModbusRegister::DISCRETE_INPUT, 10);
    TModbusRegister holding22(1, TModbusRegister::HOLDING_REGISTER, 22);
    TModbusRegister input33(1, TModbusRegister::INPUT_REGISTER, 33);

    ModbusClient->AddRegister(coil0);
    ModbusClient->AddRegister(coil1);
    ModbusClient->AddRegister(discrete10);
    ModbusClient->AddRegister(holding22);
    ModbusClient->AddRegister(input33);

    ModbusClient->Connect();

    Note() << "Cycle()";
    ModbusClient->Cycle();

    Slave->Coils[1] = 1;
    Slave->Discrete[10] = 1;
    Slave->Holding[22] = 4242;
    Slave->Input[33] = 42000;

    Note() << "Cycle()";
    ModbusClient->Cycle();

    EXPECT_EQ(0, ModbusClient->GetRawValue(coil0));
    EXPECT_EQ(1, ModbusClient->GetRawValue(coil1));
    EXPECT_EQ(1, ModbusClient->GetRawValue(discrete10));
    EXPECT_EQ(4242, ModbusClient->GetRawValue(holding22));
    EXPECT_EQ(42000, ModbusClient->GetRawValue(input33));
}

TEST_F(ModbusClientTest, Write)
{
    TModbusRegister coil1(1, TModbusRegister::COIL, 1);
    TModbusRegister holding20(1, TModbusRegister::HOLDING_REGISTER, 20);
    ModbusClient->AddRegister(coil1);
    ModbusClient->AddRegister(holding20);

    Note() << "Cycle()";
    ModbusClient->Cycle();

    ModbusClient->SetTextValue(coil1, "1");
    ModbusClient->SetTextValue(holding20, "4242");

    for (int i = 0; i < 3; ++i) {
        Note() << "Cycle()";
        ModbusClient->Cycle();

        EXPECT_EQ(1, ModbusClient->GetRawValue(coil1));
        EXPECT_EQ(1, Slave->Coils[1]);
        EXPECT_EQ(4242, ModbusClient->GetRawValue(holding20));
        EXPECT_EQ(4242, Slave->Holding[20]);
    }
}

TEST_F(ModbusClientTest, S8)
{
    TModbusRegister holding20(1, TModbusRegister::HOLDING_REGISTER, 20, TModbusRegister::S8);
    TModbusRegister input30(1, TModbusRegister::INPUT_REGISTER, 30, TModbusRegister::S8);
    ModbusClient->AddRegister(holding20);
    ModbusClient->AddRegister(input30);

    Note() << "server -> client: 10, 20";
    Slave->Holding[20] = 10;
    Slave->Input[30] = 20;
    Note() << "Cycle()";
    ModbusClient->Cycle();
    EXPECT_EQ(10, ModbusClient->GetRawValue(holding20));
    EXPECT_EQ(20, ModbusClient->GetRawValue(input30));

    Note() << "server -> client: -2, -3";
    Slave->Holding[20] = 254;
    Slave->Input[30] = 253;
    Note() << "Cycle()";
    ModbusClient->Cycle();
    EXPECT_EQ(-2, ModbusClient->GetRawValue(holding20));
    EXPECT_EQ(-3, ModbusClient->GetRawValue(input30));

    Note() << "client -> server: 10";
    ModbusClient->SetTextValue(holding20, "10");
    Note() << "Cycle()";
    ModbusClient->Cycle();
    EXPECT_EQ(10, ModbusClient->GetRawValue(holding20));
    EXPECT_EQ(10, Slave->Holding[20]);

    Note() << "client -> server: -2";
    ModbusClient->SetTextValue(holding20, "-2");
    Note() << "Cycle()";
    ModbusClient->Cycle();
    EXPECT_EQ(-2, ModbusClient->GetRawValue(holding20));
    EXPECT_EQ(254, Slave->Holding[20]);
}
