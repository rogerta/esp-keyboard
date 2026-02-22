
#include <iostream>
#include <unit_tests.h>

#include "ps2_protocol.h"

// This is here to test the global macro.  The variables are not used.
namespace {
PS2P_GLOBAL(dummy);
PS2P_GLOBAL(toto);
}

///////////////////////////////////////////////////////////////////////////////
// Test the begin() method.

class PS2ProtocolBeginTests : public testing::TestCase {
 protected:
  PS2P_DECLARE(PS2ProtocolBeginTests, protocol_);
};

PS2P_IMPLEMENT(PS2ProtocolBeginTests, protocol_);

TEST_F(PS2ProtocolBeginTests, Begin) {
  EXPECT_TRUE(protocol_.begin(2, 3));
}

TEST_F(PS2ProtocolBeginTests, BeginTwice) {
  EXPECT_TRUE(protocol_.begin(2, 4));
  EXPECT_FALSE(protocol_.begin(3, 5));
}

TEST_F(PS2ProtocolBeginTests, BeginSamePin) {
  EXPECT_FALSE(protocol_.begin(2, 2));
}

TEST_F(PS2ProtocolBeginTests, BeginClockSupportsIsr) {
  // The arduino mock framework supports interrupts only on pins 2 and 3,
  // so for this test I just make sure to use a pin other than those.
  EXPECT_FALSE(protocol_.begin(4, 3));
}

TEST_F(PS2ProtocolBeginTests, NoneAvailable) {
  EXPECT_TRUE(protocol_.begin(2, 3));
}

///////////////////////////////////////////////////////////////////////////////
// Test the ISR handler when receiving bytes from the device.

class PS2ProtocolReceiveTests : public testing::TestCase {
 protected:
  void SendByte(byte b) {
    SendByte(0, b, -1, 1);
  }
  void SendByte(int start_bit, byte b, int parity_bit, int stop_bit) {
    // Send start bit.
    protocol_.callIsrHandlerForTesting(start_bit);
    // Send data bits.
    int parity = 1;
    for (int i = 0; i < 8; ++i) {
      int bit = (b & (1 << i)) ? HIGH : LOW;
      if (bit)
        parity ^= 1;
      protocol_.callIsrHandlerForTesting(bit);
    }
    // Send parity bit.
    if (parity_bit != -1)
      parity = parity_bit;
    protocol_.callIsrHandlerForTesting(parity);
    // Send stop bit.
    protocol_.callIsrHandlerForTesting(stop_bit);
  }
  PS2P_DECLARE(PS2ProtocolTests, protocol_);
 private:
  void SetUp() {
    // Uncomment if needed to see extra messages during tests.
    //protocol_.setErrorHandler(ErrorHandler);
    protocol_.begin(2, 3);
  }
  static void ErrorHandler(const __FlashStringHelper* error) {
    std::cout << "*** PS2 protocol error: " << error << std::endl;
  }
};

PS2P_IMPLEMENT(PS2ProtocolReceiveTests, protocol_);

TEST_F(PS2ProtocolReceiveTests, SendOneByteEvenParity) {
  SendByte(0x12);
  EXPECT_EQ(PS2Protocol::WAIT_R_START, protocol_.getStateForTesting());
  EXPECT_EQ(1, protocol_.available());
}

TEST_F(PS2ProtocolReceiveTests, SendOneByteOddParity) {
  SendByte(0x13);
  EXPECT_EQ(PS2Protocol::WAIT_R_START, protocol_.getStateForTesting());
  EXPECT_EQ(1, protocol_.available());
}

TEST_F(PS2ProtocolReceiveTests, BadStartBit) {
  SendByte(1, 0x12, -1, 1);
  EXPECT_EQ(0, protocol_.available());
}

TEST_F(PS2ProtocolReceiveTests, BadParityBit) {
  SendByte(0, 0x12, 0, 1);
  EXPECT_EQ(0, protocol_.available());
}

TEST_F(PS2ProtocolReceiveTests, BadStopBit) {
  SendByte(0, 0x12, -1, 0);
  EXPECT_EQ(0, protocol_.available());
}

TEST_F(PS2ProtocolReceiveTests, Buffer) {
  for (int i = 0; i < PS2Protocol::kBufferSize; ++i) {
    EXPECT_EQ(i, protocol_.available());
    SendByte(i);
  }
  EXPECT_EQ(PS2Protocol::kBufferSize, protocol_.available());

  for (int i = 0; i < PS2Protocol::kBufferSize; ++i) {
    EXPECT_EQ(i, protocol_.read());
  }
}

TEST_F(PS2ProtocolReceiveTests, BufferOverflow) {
  // Send one more byte than the buffer can hold.
  for (int i = 0; i < PS2Protocol::kBufferSize + 1; ++i) {
    EXPECT_EQ(i, protocol_.available());
    SendByte(i);
  }
  EXPECT_EQ(PS2Protocol::kBufferSize, protocol_.available());

  // Make sure that the dropped byte was the last one.
  for (int i = 0; i < PS2Protocol::kBufferSize; ++i) {
    EXPECT_EQ(i, protocol_.read());
  }
}

TEST_F(PS2ProtocolReceiveTests, NoAvailableAftetEnd) {
  SendByte(0x12);
  protocol_.end();
  EXPECT_FALSE(protocol_.available());
}

///////////////////////////////////////////////////////////////////////////////
// Test the ISR handler when sending bytes from the device.

class PS2ProtocolSendTests : public testing::TestCase {
 public:
  PS2ProtocolSendTests() : error_handler_called_(false) {}

 protected:
  bool SendByte(byte b, byte expected_parity) {
    protocol_.write(b);
    EXPECT_EQ(PS2Protocol::WAIT_S_DATA0, protocol_.getStateForTesting());

    // Generate 8 clock bits to send the byte, plus one for the parity bit and
    // another for the stop bit.
    for (int i = 0; i < 10; ++i) {
      GenerateClock();
      if (i < 8) {
        EXPECT_EQ(bitRead(b, i), digitalRead(3));
      } else if (i == 8) {
        // Check parity bit.
        EXPECT_EQ(expected_parity, digitalRead(3));
      } else if (i == 9) {
        // After stop bit, data pin should be an input again.
        EXPECT_EQ(INPUT_PULLUP, arduino::mock::GetPinMode(3));
      }
    }

    return true;
  }
  void GenerateClock() {
    protocol_.callIsrHandlerForTesting(LOW);
  }
  void GenerateAck(int bit) {
    protocol_.callIsrHandlerForTesting(bit);
  }

  bool ErrorHandlerCalled() { return error_handler_called_; }

  PS2P_DECLARE(PS2ProtocolTests, protocol_);
 private:
  void SetUp() override {
    protocol_.begin(2, 3);
  }

  bool error_handler_called_;
};

PS2P_IMPLEMENT(PS2ProtocolSendTests, protocol_);

TEST_F(PS2ProtocolSendTests, SendEvenParity) {
  SendByte(0x12, HIGH);
  EXPECT_FALSE(has_error());
  GenerateAck(LOW);

  // Make sure protocol is now idle.
  EXPECT_EQ(PS2Protocol::WAIT_R_START, protocol_.getStateForTesting());
}

TEST_F(PS2ProtocolSendTests, SendOddParity) {
  SendByte(0x23, LOW);
  EXPECT_FALSE(has_error());
  GenerateAck(LOW);

  // Make sure protocol is now idle.
  EXPECT_EQ(PS2Protocol::WAIT_R_START, protocol_.getStateForTesting());
}

TEST_F(PS2ProtocolSendTests, BadAck) {
  SendByte(0x34, LOW);
  EXPECT_FALSE(has_error());
  GenerateAck(HIGH);

  // TODO: check that error handler is actually called.  Will require
  // refactoring of error handling routine.  Needs to be an interface and
  // not just a callback function.

  // Make sure protocol is now idle.
  EXPECT_EQ(PS2Protocol::WAIT_R_START, protocol_.getStateForTesting());
}
