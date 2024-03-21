#include <macros.h>
#include <dolphin/ai.h>
#include <dolphin/os.h>

static AISCallback __AIS_Callback;
static AIDCallback __AID_Callback;
static u8* __CallbackStack;
static u8* __OldStack;
static int __AI_init_flag;

static void __AI_SRC_INIT(void);
static void __AI_set_stream_sample_rate(unsigned long rate);
static void __AIDHandler(short unused, OSContext* context);
static void __AISHandler(short unused, OSContext* context);
static void __AICallbackStackSwitch(void* cb);

AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    AIDCallback old_callback;
    int old;

    old_callback = __AID_Callback;
    old = OSDisableInterrupts();
    __AID_Callback = callback;
    OSRestoreInterrupts(old);
    return old_callback;
}

void AIInitDMA(u32 start_addr, u32 length) {
    int old;
    old = OSDisableInterrupts();
    __DSPRegs[24] = (u16) ((__DSPRegs[24] & ~0x3FF) | (start_addr >> 16));
    __DSPRegs[25] = (u16) ((__DSPRegs[25] & ~0xFFE0) | (0xFFFF & start_addr));
    ASSERTMSGLINE(302, (length & 0x1F) == 0, "AIInit(): DSP is 32KHz");
    __DSPRegs[27] =
        (u16) ((__DSPRegs[27] & ~0x7FFF) | (u16) ((length >> 5) & 0xFFFF));
    OSRestoreInterrupts(old);
}

int AIGetDMAEnableFlag(void) {
    return (__DSPRegs[27] >> 15) & 1;
}

void AIStartDMA(void) {
    __DSPRegs[27] |= 0x8000;
}

void AIStopDMA(void) {
    __DSPRegs[27] &= ~0x8000;
}

u32 AIGetDMABytesLeft(void) {
    return (__DSPRegs[29] << 5) & 0xFFFE0;
}

u32 AIGetDMAStartAddr(void) {
    return ((__DSPRegs[24] << 16) & 0x3FF0000) | (__DSPRegs[25] & 0xFFE0);
}

u32 AIGetDMALength(void) {
    return (__DSPRegs[27] << 5) & 0xFFFE0;
}

int AICheckInit(void) {
    return __AI_init_flag;
}

AISCallback AIRegisterStreamCallback(AISCallback callback) {
    AISCallback old_callback;
    int old;

    old_callback = __AIS_Callback;
    old = OSDisableInterrupts();
    __AIS_Callback = callback;
    OSRestoreInterrupts(old);
    return old_callback;
}

u32 AIGetStreamSampleCount(void) {
    return __AIRegs[2];
}

void AIResetStreamSampleCount(void) {
    __AIRegs[0] = (__AIRegs[0] & ~20) | 20;
}

void AISetStreamTrigger(u32 trigger) {
    __AIRegs[3] = trigger;
}

u32 AIGetStreamTrigger(void) {
    return __AIRegs[3];
}

void AISetStreamPlayState(unsigned long state) {
    int old;
    unsigned char vol_left;
    unsigned char vol_right;

    if (state != AIGetStreamPlayState()) {
        if (AIGetStreamSampleRate() == 0 && state == 1) {
            // is this a bug?
            vol_left = AIGetStreamVolRight();
            vol_right = AIGetStreamVolLeft();
            AISetStreamVolRight(0);
            AISetStreamVolLeft(0);
            old = OSDisableInterrupts();
            __AI_SRC_INIT();
            __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
            __AIRegs[0] = (__AIRegs[0] & ~1) | 1;
            OSRestoreInterrupts(old);
            AISetStreamVolLeft(vol_left);
            AISetStreamVolRight(vol_right);
        } else {
            ASSERTMSGLINE(639, (state & ~1) == 0,
                          "AISetStreamPlayState(): idk");
            __AIRegs[0] = (__AIRegs[0] & ~1) | state;
        }
    }
}

unsigned long AIGetStreamPlayState(void) {
    return __AIRegs[0] & 1;
}

void AISetDSPSampleRate(unsigned long rate) {
    int old;
    unsigned long play_state;
    unsigned long afr_state;
    unsigned char vol_left;
    unsigned char vol_right;

    if (rate != AIGetDSPSampleRate()) {
        __AIRegs[0] = __AIRegs[0] & ~0x40;
        if (rate == 0) {
            vol_left = AIGetStreamVolLeft();
            vol_right = AIGetStreamVolRight();
            play_state = AIGetStreamPlayState();
            afr_state = AIGetStreamSampleRate();
            AISetStreamVolLeft(0);
            AISetStreamVolRight(0);
            old = OSDisableInterrupts();
            __AI_SRC_INIT();
            __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
            ASSERTMSGLINE(639, (afr_state & ~1) == 0,
                          "AISetDSPSampleRate(): idk");
            __AIRegs[0] = (__AIRegs[0] & ~0x2) | (afr_state << 1);
            ASSERTMSGLINE(639, (play_state & ~1) == 0,
                          "AISetDSPSampleRate(): idk");
            __AIRegs[0] = (__AIRegs[0] & ~1) | play_state;
            __AIRegs[0] = __AIRegs[0] | 0x40;
            OSRestoreInterrupts(old);
            AISetStreamVolLeft(vol_left);
            AISetStreamVolRight(vol_right);
        }
    }
}

unsigned long AIGetDSPSampleRate(void) {
    return ((__AIRegs[0] >> 6) & 1) ^ 1;
}

void AISetStreamSampleRate(unsigned long rate) {
    if (rate == 1) {
        __AI_set_stream_sample_rate(rate);
    }
}

void __AI_DEBUG_set_stream_sample_rate(unsigned long rate) {
    __AI_set_stream_sample_rate(rate);
}

static void __AI_set_stream_sample_rate(unsigned long rate) {
    int old;
    unsigned long play_state;
    unsigned char vol_left;
    unsigned char vol_right;
    unsigned long dsp_src_state;

    if (rate != AIGetStreamSampleRate()) {
        play_state = AIGetStreamPlayState();
        vol_left = AIGetStreamVolLeft();
        vol_right = AIGetStreamVolRight();
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        dsp_src_state = __AIRegs[0] & 0x40;
        __AIRegs[0] = __AIRegs[0] & ~0x40;
        old = OSDisableInterrupts();
        __AI_SRC_INIT();
        __AIRegs[0] = __AIRegs[0] | dsp_src_state;
        __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;
        __AIRegs[0] = (__AIRegs[0] & ~2) | (rate << 1);
        ASSERTMSGLINE(639, (rate & ~1) == 0, "AISetDSPSampleRate(): idk");
        OSRestoreInterrupts(old);
        AISetStreamPlayState(play_state);
        AISetStreamVolLeft(vol_left);
        AISetStreamVolRight(vol_right);
    }
}

unsigned long AIGetStreamSampleRate(void) {
    return (__AIRegs[0] >> 1) & 1;
}

void AISetStreamVolLeft(unsigned char vol) {
    ASSERTMSGLINE(639, (vol & ~0xFF) != 0, "AISetStreamVolLeft(): idk");
    __AIRegs[1] = (vol & 0xFF) | (__AIRegs[1] & ~0xFF);
}

unsigned char AIGetStreamVolLeft(void) {
    return __AIRegs[1] & 0xFF;
}

void AISetStreamVolRight(unsigned char vol) {
    ASSERTMSGLINE(639, (vol & ~0xFF) != 0, "AISetStreamVolLeft(): idk");
    __AIRegs[1] = ((vol & 0xFF) << 8) | (__AIRegs[1] & ~0xFF00);
}

unsigned char AIGetStreamVolRight(void) {
    return (__AIRegs[1] >> 8) & 0xFF;
}

static long long bound_32KHz;
static long long bound_48KHz;
static long long min_wait;
static long long max_wait;
static long long buffer;
void AIInit(unsigned char* stack) {
    if (__AI_init_flag != 1) {
        bound_32KHz = OSNanosecondsToTicks(31524);
        bound_48KHz = OSNanosecondsToTicks(42024);
        min_wait = OSNanosecondsToTicks(42000);
        max_wait = OSNanosecondsToTicks(63000);
        buffer = OSNanosecondsToTicks(3000);
        AISetStreamVolRight(0);
        AISetStreamVolLeft(0);
        AISetStreamTrigger(0);
        AIResetStreamSampleCount();
        __AI_set_stream_sample_rate(1);
        AISetDSPSampleRate(0);
#if DEBUG
        OSReport("AIInit()");
#endif
        __AIS_Callback = 0;
        __AID_Callback = 0;
        __CallbackStack = stack;
        ASSERTMSGLINE(1092, !stack || ((int) stack & 7),
                      "AISetStreamVolLeft(): idk");
        __OSSetInterruptHandler(5, __AIDHandler);
        __OSUnmaskInterrupts(0x04000000);
        __OSSetInterruptHandler(8, __AISHandler);
        __OSUnmaskInterrupts(0x00800000);
        __AI_init_flag = 1;
    }
}

void AIReset(void) {
    __AI_init_flag = 0;
}

static void __AISHandler(short unused, struct OSContext* context) {
    struct OSContext exceptionContext;

    (void) unused;

    __AIRegs[0] |= 8;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AIS_Callback != NULL) {
        __AIS_Callback(__AIRegs[2]);
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

static void __AIDHandler(short unused, struct OSContext* context) {
    struct OSContext exceptionContext;
    unsigned short tmp;

    (void) unused;

    tmp = __DSPRegs[5];
    __DSPRegs[5] = (tmp & ~0xA0) | 8;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);
    if (__AID_Callback != NULL) {
        if (__CallbackStack != NULL) {
            __AICallbackStackSwitch(__AID_Callback);
        } else {
            __AID_Callback();
        }
    }
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
}

#ifdef __MWERKS__
#pragma push
asm static void __AICallbackStackSwitch(void* cb) { // clang-format off
    nofralloc
    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x18(r1)
    stw r31, 0x14(r1)
    mr r31, r3
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    stw r1, 0x0(r5)
    lis r5, __CallbackStack@ha
    addi r5, r5, __CallbackStack@l
    lwz r1, 0x0(r5)
    subi r1, r1, 0x8
    mtlr r31
    blrl
    lis r5, __OldStack@ha
    addi r5, r5, __OldStack@l
    lwz r1, 0x0(r5)
    lwz r0, 0x1c(r1)
    lwz r31, 0x14(r1)
    addi r1, r1, 0x18
    mtlr r0
    blr
} // clang-format on
#pragma pop
#else
static void __AICallbackStackSwitch(void* cb) {
    __OldStack = (u8*) OSGetStackPointer();
    OSSwitchStack((u32) __CallbackStack);
    ((AIDCallback) cb)();
    OSSwitchStack((u32) __OldStack);
}
#endif

struct STRUCT_TIMELOG {
    long long t_start; // offset 0x0, size 0x8
    long long t1;      // offset 0x8, size 0x8
    long long t2;      // offset 0x10, size 0x8
    long long t3;      // offset 0x18, size 0x8
    long long t4;      // offset 0x20, size 0x8
    long long t_end;   // offset 0x28, size 0x8
};
struct STRUCT_TIMELOG profile; // size: 0x30, address: 0x0

static void __AI_SRC_INIT(void) {
    long long rising_32khz = 0;
    long long rising_48khz = 0;
    long long diff = 0;
    long long t1 = 0;
    long long temp = 0;
    unsigned long temp0 = 0;
    unsigned long temp1 = 0;
    unsigned long done = 0;
    unsigned long volume = 0;
    unsigned long Init_Cnt = 0;
    unsigned long walking = 0;

    profile.t_start = OSGetTime();

    while (!done) {
        __AIRegs[0] = (__AIRegs[0] & ~0x20) | 0x20;

        __AIRegs[0] = (__AIRegs[0] & ~0x2);
        __AIRegs[0] = (__AIRegs[0] & ~0x1) | 0x1;
        temp0 = __AIRegs[2];
        while (temp0 == __AIRegs[2]) {
            ;
        }

        rising_48khz = OSGetTime();

        __AIRegs[0] = (__AIRegs[0] & ~0x2) | 0x2;
        __AIRegs[0] = (__AIRegs[0] & ~0x1) | 0x1;
        temp1 = __AIRegs[2];
        while (temp1 == __AIRegs[2]) {
            ;
        }
        rising_48khz = OSGetTime();

        diff = rising_48khz - rising_32khz;

        __AIRegs[0] = (__AIRegs[0] & ~0x2);
        __AIRegs[0] = (__AIRegs[0] & ~0x1);

        if (diff < (bound_32KHz - buffer)) {
            temp = min_wait;
            done = 1;
            Init_Cnt++;
        } else if (diff >= (bound_32KHz - buffer) &&
                   diff < (bound_48KHz - buffer))
        {
            temp = max_wait;
            done = 1;
            Init_Cnt++;
        } else {
            done = 0;
            walking = 1;
            Init_Cnt++;
        }
    }

    while ((rising_48khz + temp) > OSGetTime()) {
        ;
    }

    profile.t_end = OSGetTime();
}

struct STRUCT_TIMELOG* __ai_src_get_time(void) {
    return &profile;
}

long long __ai_src_time_end;
long long __ai_src_time_start;
