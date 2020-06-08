/* CPU Information (v1)
 * Portable Snippets - https://github.com/nemequ/portable-snippets
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   https://creativecommons.org/publicdomain/zero/1.0/
 */

#if !defined(PSNIP_CPU__H)
#define PSNIP_CPU__H

#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
#  define PSNIP_CPU_ARCH_X86_64
#elif defined(__i686__) || defined(__i586__) || defined(__i486__) || defined(__i386__) || defined(__i386) || defined(_M_IX86) || defined(_X86_) || defined(__THW_INTEL__)
#  define PSNIP_CPU_ARCH_X86
#elif defined(__arm__) || defined(_M_ARM)
#  define PSNIP_CPU_ARCH_ARM
#elif defined(__aarch64__)
#  define PSNIP_CPU_ARCH_ARM64
#endif

#if defined(__cplusplus)
extern "C" {
#endif

enum PSnipCPUFeature {
    PSNIP_CPU_FEATURE_NONE                = 0,

    PSNIP_CPU_FEATURE_CPU_MASK            = 0x1f000000,
    PSNIP_CPU_FEATURE_X86                 = 0x01000000,
    PSNIP_CPU_FEATURE_ARM                 = 0x04000000,

    /* x86 CPU features are constructed as:
     *
     *   (PSNIP_CPU_FEATURE_X86 | (eax << 16) | (ret_reg << 8) | (bit_position)
     *
     * For example, SSE3 is determined by the fist bit in the ECX
     * register for a CPUID call with EAX=1, so we get:
     *
     *   PSNIP_CPU_FEATURE_X86 | (1 << 16) | (2 << 8) | (0) = 0x01010200
     *
     * We should have information for inputs of EAX=0-7 w/ ECX=0.
     */
    PSNIP_CPU_FEATURE_X86_FPU             = 0x01010300,
    PSNIP_CPU_FEATURE_X86_VME             = 0x01010301,
    PSNIP_CPU_FEATURE_X86_DE              = 0x01010302,
    PSNIP_CPU_FEATURE_X86_PSE             = 0x01010303,
    PSNIP_CPU_FEATURE_X86_TSC             = 0x01010304,
    PSNIP_CPU_FEATURE_X86_MSR             = 0x01010305,
    PSNIP_CPU_FEATURE_X86_PAE             = 0x01010306,
    PSNIP_CPU_FEATURE_X86_MCE             = 0x01010307,
    PSNIP_CPU_FEATURE_X86_CX8             = 0x01010308,
    PSNIP_CPU_FEATURE_X86_APIC            = 0x01010309,
    PSNIP_CPU_FEATURE_X86_SEP             = 0x0101030b,
    PSNIP_CPU_FEATURE_X86_MTRR            = 0x0101030c,
    PSNIP_CPU_FEATURE_X86_PGE             = 0x0101030d,
    PSNIP_CPU_FEATURE_X86_MCA             = 0x0101030e,
    PSNIP_CPU_FEATURE_X86_CMOV            = 0x0101030f,
    PSNIP_CPU_FEATURE_X86_PAT             = 0x01010310,
    PSNIP_CPU_FEATURE_X86_PSE_36          = 0x01010311,
    PSNIP_CPU_FEATURE_X86_PSN             = 0x01010312,
    PSNIP_CPU_FEATURE_X86_CLFSH           = 0x01010313,
    PSNIP_CPU_FEATURE_X86_DS              = 0x01010314,
    PSNIP_CPU_FEATURE_X86_ACPI            = 0x01010316,
    PSNIP_CPU_FEATURE_X86_MMX             = 0x01010317,
    PSNIP_CPU_FEATURE_X86_FXSR            = 0x01010318,
    PSNIP_CPU_FEATURE_X86_SSE             = 0x01010319,
    PSNIP_CPU_FEATURE_X86_SSE2            = 0x0101031a,
    PSNIP_CPU_FEATURE_X86_SS              = 0x0101031b,
    PSNIP_CPU_FEATURE_X86_HTT             = 0x0101031c,
    PSNIP_CPU_FEATURE_X86_TM              = 0x0101031d,
    PSNIP_CPU_FEATURE_X86_IA64            = 0x0101031e,
    PSNIP_CPU_FEATURE_X86_PBE             = 0x0101031f,

    PSNIP_CPU_FEATURE_X86_SSE3            = 0x01010200,
    PSNIP_CPU_FEATURE_X86_PCLMULQDQ       = 0x01010201,
    PSNIP_CPU_FEATURE_X86_DTES64          = 0x01010202,
    PSNIP_CPU_FEATURE_X86_MONITOR         = 0x01010203,
    PSNIP_CPU_FEATURE_X86_DS_CPL          = 0x01010204,
    PSNIP_CPU_FEATURE_X86_VMX             = 0x01010205,
    PSNIP_CPU_FEATURE_X86_SMX             = 0x01010206,
    PSNIP_CPU_FEATURE_X86_EST             = 0x01010207,
    PSNIP_CPU_FEATURE_X86_TM2             = 0x01010208,
    PSNIP_CPU_FEATURE_X86_SSSE3           = 0x01010209,
    PSNIP_CPU_FEATURE_X86_CNXT_ID         = 0x0101020a,
    PSNIP_CPU_FEATURE_X86_SDBG            = 0x0101020b,
    PSNIP_CPU_FEATURE_X86_FMA             = 0x0101020c,
    PSNIP_CPU_FEATURE_X86_CX16            = 0x0101020d,
    PSNIP_CPU_FEATURE_X86_XTPR            = 0x0101020e,
    PSNIP_CPU_FEATURE_X86_PDCM            = 0x0101020f,
    PSNIP_CPU_FEATURE_X86_PCID            = 0x01010211,
    PSNIP_CPU_FEATURE_X86_DCA             = 0x01010212,
    PSNIP_CPU_FEATURE_X86_SSE4_1          = 0x01010213,
    PSNIP_CPU_FEATURE_X86_SSE4_2          = 0x01010214,
    PSNIP_CPU_FEATURE_X86_X2APIC          = 0x01010215,
    PSNIP_CPU_FEATURE_X86_MOVBE           = 0x01010216,
    PSNIP_CPU_FEATURE_X86_POPCNT          = 0x01010217,
    PSNIP_CPU_FEATURE_X86_TSC_DEADLINE    = 0x01010218,
    PSNIP_CPU_FEATURE_X86_AES             = 0x01010219,
    PSNIP_CPU_FEATURE_X86_XSAVE           = 0x0101021a,
    PSNIP_CPU_FEATURE_X86_OSXSAVE         = 0x0101021b,
    PSNIP_CPU_FEATURE_X86_AVX             = 0x0101021c,
    PSNIP_CPU_FEATURE_X86_F16C            = 0x0101021d,
    PSNIP_CPU_FEATURE_X86_RDRND           = 0x0101021e,
    PSNIP_CPU_FEATURE_X86_HYPERVISOR      = 0x0101021f,

    PSNIP_CPU_FEATURE_X86_FSGSBASE        = 0x01070100,
    PSNIP_CPU_FEATURE_X86_TSC_ADJ         = 0x01070101,
    PSNIP_CPU_FEATURE_X86_SGX             = 0x01070102,
    PSNIP_CPU_FEATURE_X86_BMI1            = 0x01070103,
    PSNIP_CPU_FEATURE_X86_HLE             = 0x01070104,
    PSNIP_CPU_FEATURE_X86_AVX2            = 0x01070105,
    PSNIP_CPU_FEATURE_X86_SMEP            = 0x01070107,
    PSNIP_CPU_FEATURE_X86_BMI2            = 0x01070108,
    PSNIP_CPU_FEATURE_X86_ERMS            = 0x01070109,
    PSNIP_CPU_FEATURE_X86_INVPCID         = 0x0107010a,
    PSNIP_CPU_FEATURE_X86_RTM             = 0x0107010b,
    PSNIP_CPU_FEATURE_X86_PQM             = 0x0107010c,
    PSNIP_CPU_FEATURE_X86_MPX             = 0x0107010e,
    PSNIP_CPU_FEATURE_X86_PQE             = 0x0107010f,
    PSNIP_CPU_FEATURE_X86_AVX512F         = 0x01070110,
    PSNIP_CPU_FEATURE_X86_AVX512DQ        = 0x01070111,
    PSNIP_CPU_FEATURE_X86_RDSEED          = 0x01070112,
    PSNIP_CPU_FEATURE_X86_ADX             = 0x01070113,
    PSNIP_CPU_FEATURE_X86_SMAP            = 0x01070114,
    PSNIP_CPU_FEATURE_X86_AVX512IFMA      = 0x01070115,
    PSNIP_CPU_FEATURE_X86_PCOMMIT         = 0x01070116,
    PSNIP_CPU_FEATURE_X86_CLFLUSHOPT      = 0x01070117,
    PSNIP_CPU_FEATURE_X86_CLWB            = 0x01070118,
    PSNIP_CPU_FEATURE_X86_INTEL_PT        = 0x01070119,
    PSNIP_CPU_FEATURE_X86_AVX512PF        = 0x0107011a,
    PSNIP_CPU_FEATURE_X86_AVX512ER        = 0x0107011b,
    PSNIP_CPU_FEATURE_X86_AVX512CD        = 0x0107011c,
    PSNIP_CPU_FEATURE_X86_SHA             = 0x0107011d,
    PSNIP_CPU_FEATURE_X86_AVX512BW        = 0x0107011e,
    PSNIP_CPU_FEATURE_X86_AVX512VL        = 0x0107011f,

    PSNIP_CPU_FEATURE_X86_PREFETCHWT1     = 0x01070200,
    PSNIP_CPU_FEATURE_X86_AVX512VBMI      = 0x01070201,
    PSNIP_CPU_FEATURE_X86_UMIP            = 0x01070202,
    PSNIP_CPU_FEATURE_X86_PKU             = 0x01070203,
    PSNIP_CPU_FEATURE_X86_OSPKE           = 0x01070204,
    PSNIP_CPU_FEATURE_X86_AVX512VPOPCNTDQ = 0x0107020e,
    PSNIP_CPU_FEATURE_X86_RDPID           = 0x01070215,
    PSNIP_CPU_FEATURE_X86_SGX_LC          = 0x0107021e,

    PSNIP_CPU_FEATURE_X86_AVX512_4VNNIW   = 0x01070302,
    PSNIP_CPU_FEATURE_X86_AVX512_4FMAPS   = 0x01070303,

    PSNIP_CPU_FEATURE_ARM_SWP             = PSNIP_CPU_FEATURE_ARM | 1,
    PSNIP_CPU_FEATURE_ARM_HALF            = PSNIP_CPU_FEATURE_ARM | 2,
    PSNIP_CPU_FEATURE_ARM_THUMB           = PSNIP_CPU_FEATURE_ARM | 3,
    PSNIP_CPU_FEATURE_ARM_26BIT           = PSNIP_CPU_FEATURE_ARM | 4,
    PSNIP_CPU_FEATURE_ARM_FAST_MULT       = PSNIP_CPU_FEATURE_ARM | 5,
    PSNIP_CPU_FEATURE_ARM_FPA             = PSNIP_CPU_FEATURE_ARM | 6,
    PSNIP_CPU_FEATURE_ARM_VFP             = PSNIP_CPU_FEATURE_ARM | 7,
    PSNIP_CPU_FEATURE_ARM_EDSP            = PSNIP_CPU_FEATURE_ARM | 8,
    PSNIP_CPU_FEATURE_ARM_JAVA            = PSNIP_CPU_FEATURE_ARM | 9,
    PSNIP_CPU_FEATURE_ARM_IWMMXT          = PSNIP_CPU_FEATURE_ARM | 10,
    PSNIP_CPU_FEATURE_ARM_CRUNCH          = PSNIP_CPU_FEATURE_ARM | 11,
    PSNIP_CPU_FEATURE_ARM_THUMBEE         = PSNIP_CPU_FEATURE_ARM | 12,
    PSNIP_CPU_FEATURE_ARM_NEON            = PSNIP_CPU_FEATURE_ARM | 13,
    PSNIP_CPU_FEATURE_ARM_VFPV3           = PSNIP_CPU_FEATURE_ARM | 14,
    PSNIP_CPU_FEATURE_ARM_VFPV3D16        = PSNIP_CPU_FEATURE_ARM | 15,
    PSNIP_CPU_FEATURE_ARM_TLS             = PSNIP_CPU_FEATURE_ARM | 16,
    PSNIP_CPU_FEATURE_ARM_VFPV4           = PSNIP_CPU_FEATURE_ARM | 17,
    PSNIP_CPU_FEATURE_ARM_IDIVA           = PSNIP_CPU_FEATURE_ARM | 18,
    PSNIP_CPU_FEATURE_ARM_IDIVT           = PSNIP_CPU_FEATURE_ARM | 19,
    PSNIP_CPU_FEATURE_ARM_VFPD32          = PSNIP_CPU_FEATURE_ARM | 20,
    PSNIP_CPU_FEATURE_ARM_LPAE            = PSNIP_CPU_FEATURE_ARM | 21,
    PSNIP_CPU_FEATURE_ARM_EVTSTRM         = PSNIP_CPU_FEATURE_ARM | 22,

    PSNIP_CPU_FEATURE_ARM_AES             = PSNIP_CPU_FEATURE_ARM | 0x0100 | 1,
    PSNIP_CPU_FEATURE_ARM_PMULL           = PSNIP_CPU_FEATURE_ARM | 0x0100 | 2,
    PSNIP_CPU_FEATURE_ARM_SHA1            = PSNIP_CPU_FEATURE_ARM | 0x0100 | 3,
    PSNIP_CPU_FEATURE_ARM_SHA2            = PSNIP_CPU_FEATURE_ARM | 0x0100 | 4,
    PSNIP_CPU_FEATURE_ARM_CRC32           = PSNIP_CPU_FEATURE_ARM | 0x0100 | 5
};

int psnip_cpu_count              (void);
int psnip_cpu_feature_check      (enum PSnipCPUFeature  feature);
int psnip_cpu_feature_check_many (enum PSnipCPUFeature* feature);

#if defined(__cplusplus)
}
#endif

#endif /* PSNIP_CPU__H */