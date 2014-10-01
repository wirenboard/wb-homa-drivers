#include "fake_modbus.h"

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
    ASSERT_LE(nb, MODBUS_MAX_READ_BITS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Coils.ReadRegs(Fixture, addr, nb, dest);
}

void TFakeModbusContext::WriteCoil(int addr, int value)
{
    FAIL() << "TBD WriteCoil " << addr << " " << value;
}

void TFakeModbusContext::ReadDisceteInputs(int addr, int nb, uint8_t *dest)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_BITS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Discrete.ReadRegs(Fixture, addr, nb, dest);
}

void TFakeModbusContext::ReadHoldingRegisters(int addr, int nb, uint16_t *dest)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_REGISTERS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Holding.ReadRegs(Fixture, addr, nb, dest);
}

void TFakeModbusContext::WriteHoldingRegisters(int addr, int nb, const uint16_t *data)
{
    FAIL() << "TBD WriteHoldingRegisters " << addr << " " << nb << " " << data;
}

void TFakeModbusContext::ReadInputRegisters(int addr, int nb, uint16_t *dest)
{
    ASSERT_LE(nb, MODBUS_MAX_READ_REGISTERS);
    ASSERT_TRUE(!!CurrentSlave);
    CurrentSlave->Input.ReadRegs(Fixture, addr, nb, dest);
}

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
