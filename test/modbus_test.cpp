#include <map>
#include <memory>
#include <cassert>
#include <gtest/gtest.h>
#include <modbus/modbus.h> // for constants such as MODBUS_MAX_READ_BITS

#include "testlog.h"
#include "../wb-homa-modbus/modbus_client.h"

struct TFakeSlave
{
    uint8_t Coils[65536];
    uint8_t Discrete[65536];
    uint16_t Holding[65536];
    uint16_t Input[65536];
};

typedef std::shared_ptr<TFakeSlave> PFakeSlave;

class TFakeModbusContext: public TModbusContext
{
public:
    void Connect();
    void Disconnect();
    void SetDebug(bool debug);
    void SetSlave(int slave);
    void ReadCoils(int addr, int nb, uint8_t *dest);
    void WriteCoil(int addr, int value);
    void ReadDisceteInputs(int addr, int nb, uint8_t *dest);
    void ReadHoldingRegisters(int addr, int nb, uint16_t *dest);
    void WriteHoldingRegisters(int addr, int nb, const uint16_t *data);
    void ReadInputRegisters(int addr, int nb, uint16_t *dest);
    void USleep(int usec);

    void ExpectDebug(bool debug)
    {
        ASSERT_EQ(Debug, debug);
    }

    PFakeSlave GetSlave(int slave);
    PFakeSlave AddSlave(int slave);

private:
    friend class TFakeModbusConnector;
    TFakeModbusContext(TLoggedFixture& fixture): Fixture(fixture) {}

    TLoggedFixture& Fixture;
    bool Connected = false;
    bool Debug = false;
    std::map<int, PFakeSlave> Slaves;
    PFakeSlave CurrentSlave;
};

void TFakeModbusContext::USleep(int usec)
{
    Fixture.Emit() << "USleep(" << usec << ")";
}

PFakeSlave TFakeModbusContext::GetSlave(int slave)
{
    auto it = Slaves.find(slave);
    if (it == Slaves.end()) {
        ADD_FAILURE() << "GetSlave(): slave not found, auto-creating: " << slave;
        return AddSlave(slave);
    }
    return it->second;
}

PFakeSlave TFakeModbusContext::AddSlave(int slave)
{
    Fixture.Note() << "AddSlave(" << slave << ")";
    auto it = Slaves.find(slave);
    if (it != Slaves.end()) {
        ADD_FAILURE() << "AddSlave(): slave already present: " << slave;
        return it->second;
    }

    PFakeSlave fake_slave(new TFakeSlave());
    memset(fake_slave.get(), 0, sizeof(TFakeSlave));
    Slaves[slave] = fake_slave;
    return fake_slave;
}

void TFakeModbusContext::Connect()
{
    Fixture.Emit() << "Connect()";
    ASSERT_FALSE(Connected);
    Connected = true;
}

void TFakeModbusContext::Disconnect()
{
    Fixture.Emit() << "Disconnect()";
    ASSERT_TRUE(Connected);
    Connected = false;
}

void TFakeModbusContext::SetDebug(bool debug)
{
    Fixture.Emit() << "SetDebug(" << debug << ")";
    Debug = debug;
}

void TFakeModbusContext::SetSlave(int slave)
{
    Fixture.Emit() << "SetSlave(" << slave << ")";
    CurrentSlave = GetSlave(slave);
}

void TFakeModbusContext::ReadCoils(int addr, int nb, uint8_t *dest)
{
    ASSERT_GT(nb, 0);
    ASSERT_LE(nb, MODBUS_MAX_READ_BITS);
    ASSERT_TRUE(!!CurrentSlave);
    uint8_t* src = CurrentSlave->Coils + addr;
    std::stringstream s;
    s << "ReadCoils(" << addr << ", " << nb << ", <ptr>):";
    while (nb--) {
        s << " 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)*src;
        *dest++ = *src++;
    }
    Fixture.Emit() << s.str();
}

void TFakeModbusContext::WriteCoil(int addr, int value)
{
    FAIL() << "TBD WriteCoil " << addr << " " << value;
}

void TFakeModbusContext::ReadDisceteInputs(int addr, int nb, uint8_t *dest)
{
    FAIL() << "TBD ReadDisceteInputs " << addr << " " << nb << " " << dest;
}

void TFakeModbusContext::ReadHoldingRegisters(int addr, int nb, uint16_t *dest)
{
    FAIL() << "TBD ReadHoldingRegisters " << addr << " " << nb << " " << dest;
}

void TFakeModbusContext::WriteHoldingRegisters(int addr, int nb, const uint16_t *data)
{
    FAIL() << "TBD WriteHoldingRegisters " << addr << " " << nb << " " << data;
}

void TFakeModbusContext::ReadInputRegisters(int addr, int nb, uint16_t *dest)
{
    FAIL() << "TBD ReadInputRegisters " << addr << " " << nb << " " << dest;
}

class TFakeModbusConnector: public TModbusConnector
{
public:
    TFakeModbusConnector(const TModbusConnectionSettings& expected_settings,
                         TLoggedFixture& fixture)
        : ExpectedSettings(expected_settings), Fixture(fixture) {}
    PModbusContext CreateContext(const TModbusConnectionSettings& settings);
    PModbusContext GetContext() const;
    PFakeSlave GetSlave(int slave);
    PFakeSlave AddSlave(int slave);
private:
    TModbusConnectionSettings ExpectedSettings;
    TLoggedFixture& Fixture;
    PModbusContext Context;
};

PModbusContext TFakeModbusConnector::CreateContext(const TModbusConnectionSettings& settings)
{
    EXPECT_FALSE(Context);
    EXPECT_EQ(ExpectedSettings.Device, settings.Device);
    EXPECT_EQ(ExpectedSettings.BaudRate, settings.BaudRate);
    EXPECT_EQ(ExpectedSettings.Parity, settings.Parity);
    EXPECT_EQ(ExpectedSettings.DataBits, settings.DataBits);
    EXPECT_EQ(ExpectedSettings.StopBits, settings.StopBits);
    Context = PModbusContext(new TFakeModbusContext(Fixture));
    return Context;
}

PModbusContext TFakeModbusConnector::GetContext() const
{
    if (!Context) {
        ADD_FAILURE() << "no modbus context created";
        return PModbusContext(new TFakeModbusContext(Fixture));
    }

    return Context;
}

PFakeSlave TFakeModbusConnector::GetSlave(int slave)
{
    return static_cast<TFakeModbusContext*>(GetContext().get())->GetSlave(slave);
}

PFakeSlave TFakeModbusConnector::AddSlave(int slave)
{
    return static_cast<TFakeModbusContext*>(GetContext().get())->AddSlave(slave);
}

class ModbusClientTest: public TLoggedFixture
{ 
protected:
    void SetUp();
    void TearDown();
    PModbusConnector Connector;
    PModbusClient ModbusClient;
};

void ModbusClientTest::SetUp()
{
    TModbusConnectionSettings settings("/dev/ttyWhatever", 115200, 'N', 8, 1);
    Connector = PModbusConnector(new TFakeModbusConnector(settings, *this));
    ModbusClient = PModbusClient(new TModbusClient(settings, Connector));
}

void ModbusClientTest::TearDown()
{
    ModbusClient.reset();
    Connector.reset();
    TLoggedFixture::TearDown();
}

TEST_F(ModbusClientTest, Init)
{
    PFakeSlave slave = static_cast<TFakeModbusConnector*>(Connector.get())->AddSlave(1);
    ModbusClient->SetCallback([this](const TModbusRegister& reg) {
            Emit() << "Modbus Callback: " << reg.ToString() << " becomes " << 
                ModbusClient->GetTextValue(reg);
        });
    ModbusClient->AddRegister(TModbusRegister(1, TModbusRegister::COIL, 1));

    ModbusClient->Connect();
    ModbusClient->Cycle();

    slave->Coils[1] = 1;
    ModbusClient->Cycle();
    EXPECT_EQ(1, ModbusClient->GetRawValue(TModbusRegister(1, TModbusRegister::COIL, 1)));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// TBD: replace direct usleep() call with call to a TModbusContext.USleep()
// TBD: specify per-slave register ranges

