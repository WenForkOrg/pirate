/*
 * This work was authored by Two Six Labs, LLC and is sponsored by a subcontract
 * agreement with Galois, Inc.  This material is based upon work supported by
 * the Defense Advanced Research Projects Agency (DARPA) under Contract No.
 * HR0011-19-C-0103.
 *
 * The Government has unlimited rights to use, modify, reproduce, release,
 * perform, display, or disclose computer software or computer software
 * documentation marked with this legend. Any reproduction of technical data,
 * computer software, or portions thereof marked with this legend must also
 * reproduce this marking.
 *
 * Copyright 2020 Two Six Labs, LLC.  All rights reserved.
 */

#include <errno.h>
#include <stdint.h>
#include <semaphore.h>
#include <sys/types.h>
#include <gtest/gtest.h>

namespace GAPS
{

class ChannelTest : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    virtual void WriteDataInit(ssize_t len);

    virtual void ChannelInit() = 0;

    virtual void WriterChannelOpen();
    virtual void ReaderChannelOpen();

    virtual void WriterChannelClose();
    virtual void ReaderChannelClose();

    void Run();
    void WriterTest();
    void ReaderTest();

    struct {
        int channel;
        uint8_t * buf;
        int status;
    } Writer, Reader;

    // Test lengths
    struct
    {
        ssize_t start;
        ssize_t stop;
        ssize_t step;
    } len;
    static const ssize_t DEFAULT_START_LEN = 1;
    static const ssize_t DEFAULT_STOP_LEN = 32;
    static const ssize_t DEFAULT_STEP_LEN = 1;

    // Reader writer synchronization
    sem_t sem;

    // Channel parameters
    pirate_channel_param_t param;

    // Optional inter-write delay
    uint32_t WriteDelayUs;
public:
    ChannelTest();
    static void *WriterThreadS(void *param);
    static void *ReaderThreadS(void *param);
    static const int TEST_CHANNEL = 2;
    static const int TEST_IOV_LEN = 16;

};

} // namespace GAPS
