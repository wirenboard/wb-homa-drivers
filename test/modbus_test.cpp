#include <map>
#include <memory>
#include <algorithm>
#include <cassert>
#include <gtest/gtest.h>
#include <modbus/modbus.h> // for constants such as MODBUS_MAX_READ_BITS

#include "testlog.h"
#include "../wb-homa-modbus/modbus_client.h"

struct TRegisterRange {
    TRegisterRange(int start = 0, int end = 0)
        : Start(start), End(end)
    {
        assert(End >= Start);
        assert(End - Start <= 65536);
    }

    int ValidateIndex(const std::string& name, int index) const
    {
        if (index < Start || index > End) {
            ADD_FAILURE() << name << " index is out of range: " << index <<
                " (must be " << Start << ".." << End << ")";
            return Start;
        }
        return index;
    }

    int Size() const
    {
        return End - Start;
    }

    int Start, End;
};

template<typename T>
class TRegisterSet {
public:
    TRegisterSet(const std::string& name, const TRegisterRange& range)
        : Name(name), Range(range)
    {
        // Allocate zero-filled array of values.
        // It always should contain at least one value
        // so that operator[] can always return some
        // reference, even in the case of erroneous access.
        values = new T[std::min(1, Range.Size())]();
    }
    ~TRegisterSet() {
        delete values;
    }
    T& operator[] (int index) {
        return values[Range.ValidateIndex(Name, index)];
    }
    const T& operator[] (int index) const {
        return values[Range.ValidateIndex(Name, index)];
    }
private:
    std::string Name;
    TRegisterRange Range;
    T* values;
};

struct TFakeSlave
{
    TFakeSlave(const TRegisterRange& coil_range = TRegisterRange(),
               const TRegisterRange& discrete_range = TRegisterRange(),
               const TRegisterRange& holding_range = TRegisterRange(),
               const TRegisterRange& input_range = TRegisterRange())
        : Coils("coil", coil_range),
          Discrete("discrete input", discrete_range),
          Holding("holding register", holding_range),
          Input("input register", input_range) {}
    TRegisterSet<uint8_t> Coils;
    TRegisterSet<uint8_t> Discrete;
    TRegisterSet<uint16_t> Holding;
    TRegisterSet<uint16_t> Input;
};

typedef std::shared_ptr<TFakeSlave> PFakeSlave;

class TFakeModbusContext: public TModbusContext
{
public:
    void Connect();
    void Disconnect();
    void SetDebug(bool debug);
    void SetSlave(int slave_addr);
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

    PFakeSlave GetSlave(int slave_addr);
    PFakeSlave AddSlave(int slave_addr, PFakeSlave slave);

private:
    friend class TFakeModbusConnector;
    TFakeModbusContext(TLoggedFixture& fixture): Fixture(fixture) {}

    TLoggedFixture& Fixture;
    bool Connected = false;
    bool Debug = false;
    std::map<int, PFakeSlave> Slaves;
    PFakeSlave CurrentSlave;
};

typedef std::shared_ptr<TFakeModbusContext> PFakeModbusContext;

void TFakeModbusContext::USleep(int usec)
{
    Fixture.Emit() << "USleep(" << usec << ")";
}

PFakeSlave TFakeModbusContext::GetSlave(int slave_addr)
{
    auto it = Slaves.find(slave_addr);
    if (it == Slaves.end()) {
        ADD_FAILURE() << "GetSlave(): slave not found, auto-creating: " << slave_addr;
        return AddSlave(slave_addr, PFakeSlave(new TFakeSlave));
    }
    return it->second;
}

PFakeSlave TFakeModbusContext::AddSlave(int slave_addr, PFakeSlave slave)
{
    Fixture.Note() << "AddSlave(" << slave_addr << ")";
    auto it = Slaves.find(slave_addr);
    if (it != Slaves.end()) {
        ADD_FAILURE() << "AddSlave(): slave already present: " << slave_addr;
        return it->second;
    }

    Slaves[slave_addr] = slave;
    return slave;
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
    std::stringstream s;
    s << "ReadCoils(" << addr << ", " << nb << ", <ptr>):";
    while (nb--) {
        int v = CurrentSlave->Coils[addr++];
        s << " 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)v;
        *dest++ = v;
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
    PFakeModbusContext GetContext() const;
    PFakeSlave GetSlave(int slave_addr);
    PFakeSlave AddSlave(int slave_addr, PFakeSlave slave);
    PFakeSlave AddSlave(int slave_addr,
                        const TRegisterRange& coil_range = TRegisterRange(),
                        const TRegisterRange& discrete_range = TRegisterRange(),
                        const TRegisterRange& holding_range = TRegisterRange(),
                        const TRegisterRange& input_range = TRegisterRange());
private:
    TModbusConnectionSettings ExpectedSettings;
    TLoggedFixture& Fixture;
    PFakeModbusContext Context;
};

typedef std::shared_ptr<TFakeModbusConnector> PFakeModbusConnector;

PModbusContext TFakeModbusConnector::CreateContext(const TModbusConnectionSettings& settings)
{
    EXPECT_FALSE(Context);
    EXPECT_EQ(ExpectedSettings.Device, settings.Device);
    EXPECT_EQ(ExpectedSettings.BaudRate, settings.BaudRate);
    EXPECT_EQ(ExpectedSettings.Parity, settings.Parity);
    EXPECT_EQ(ExpectedSettings.DataBits, settings.DataBits);
    EXPECT_EQ(ExpectedSettings.StopBits, settings.StopBits);
    Context = PFakeModbusContext(new TFakeModbusContext(Fixture));
    return Context;
}

PFakeModbusContext TFakeModbusConnector::GetContext() const
{
    if (!Context) {
        ADD_FAILURE() << "no modbus context created";
        return PFakeModbusContext(new TFakeModbusContext(Fixture));
    }

    return Context;
}

PFakeSlave TFakeModbusConnector::GetSlave(int slave_addr)
{
    return GetContext()->GetSlave(slave_addr);
}

PFakeSlave TFakeModbusConnector::AddSlave(int slave_addr, PFakeSlave slave)
{
    return GetContext()->AddSlave(slave_addr, slave);
}

PFakeSlave TFakeModbusConnector::AddSlave(int slave_addr,
                                          const TRegisterRange& coil_range,
                                          const TRegisterRange& discrete_range,
                                          const TRegisterRange& holding_range,
                                          const TRegisterRange& input_range)
{
    return AddSlave(slave_addr,
                    PFakeSlave(new TFakeSlave(coil_range, discrete_range,
                                              holding_range, input_range)));
}

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

// TBD: move TFakeModbusConnector & co. into fake_modbus.cpp
