/* CPU Information (v1)
 * Portable Snippets - https://github.com/nemequ/portable-snippets
 * Created by Evan Nemerson <evan@nemerson.com>
 *
 *   To the extent possible under law, the authors have waived all
 *   copyright and related or neighboring rights to this code.  For
 *   details, see the Creative Commons Zero 1.0 Universal license at
 *   https://creativecommons.org/publicdomain/zero/1.0/
 *
 * ---
 *
 * The once.h module has been dropped from this implementation
 */

#include "cpu.h"
#include <assert.h>

#if defined(_WIN32)
#  include <Windows.h>
#  define PSNIP_CPU__IMPL_WIN32
#elif defined(unix) || defined(__unix__) || defined(__unix)
#  include <unistd.h>
#  if defined(_SC_NPROCESSORS_ONLN) || defined(_SC_NPROC_ONLN)
#    define PSNIP_CPU__IMPL_SYSCONF
#  else
#    include <sys/sysctl.h>
#  endif
#endif

#if defined(PSNIP_CPU_ARCH_X86) || defined(PSNIP_CPU_ARCH_X86_64)
#  if defined(_MSC_VER)
static void psnip_cpu_getid(int func, int* data) {
  __cpuid(data, func);
}
#  else
static void psnip_cpu_getid(int func, int* data) {
  __asm__ ("cpuid"
	   : "=a" (data[0]), "=b" (data[1]), "=c" (data[2]), "=d" (data[3])
	   : "0" (func), "2" (0));
}
#  endif
#elif defined(PSNIP_CPU_ARCH_ARM) || defined(PSNIP_CPU_ARCH_ARM64)
#  if (defined(__GNUC__) && ((__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 16)))
#    define PSNIP_CPU__IMPL_GETAUXVAL
#    include <sys/auxv.h>
#  endif
#endif

#if defined(PSNIP_CPU_ARCH_X86) || defined(PSNIP_CPU_ARCH_X86_64)
static unsigned int psnip_cpuinfo[8 * 4] = { 0, };
#elif defined(PSNIP_CPU_ARCH_ARM) || defined(PSNIP_CPU_ARCH_ARM_64)
static unsigned long psnip_cpuinfo[2] = { 0, };
#endif

static void psnip_cpu_init(void) {
#if defined(PSNIP_CPU_ARCH_X86) || defined(PSNIP_CPU_ARCH_X86_64)
    int i;
  for (i = 0 ; i < 8 ; i++) {
    psnip_cpu_getid(i, (int*) &(psnip_cpuinfo[i * 4]));
  }
#elif defined(PSNIP_CPU_ARCH_ARM) || defined(PSNIP_CPU_ARCH_ARM_64)
    psnip_cpuinfo[0] = getauxval (AT_HWCAP);
  psnip_cpuinfo[1] = getauxval (AT_HWCAP2);
#endif
}

int
psnip_cpu_feature_check (enum PSnipCPUFeature feature) {
#if defined(PSNIP_CPU_ARCH_X86) || defined(PSNIP_CPU_ARCH_X86_64)
    unsigned int i, r, b;
#elif defined(PSNIP_CPU_ARCH_ARM) || defined(PSNIP_CPU_ARCH_ARM_64)
    unsigned long b, i;
#endif

#if defined(PSNIP_CPU_ARCH_X86) || defined(PSNIP_CPU_ARCH_X86_64)
    if ((feature & PSNIP_CPU_FEATURE_CPU_MASK) != PSNIP_CPU_FEATURE_X86)
    return 0;
#elif defined(PSNIP_CPU_ARCH_ARM) || defined(PSNIP_CPU_ARCH_ARM_64)
    if ((feature & PSNIP_CPU_FEATURE_CPU_MASK) != PSNIP_CPU_FEATURE_ARM)
    return 0;
#else
    return 0;
#endif

    feature &= (enum PSnipCPUFeature) ~PSNIP_CPU_FEATURE_CPU_MASK;
#if defined(_MSC_VER)
    #pragma warning(push)
#pragma warning(disable:4152)
#endif
    psnip_cpu_init();
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(PSNIP_CPU_ARCH_X86) || defined(PSNIP_CPU_ARCH_X86_64)
    i = (feature >> 16) & 0xff;
  r = (feature >>  8) & 0xff;
  b = (feature      ) & 0xff;

  if (i > 7 || r > 3 || b > 31)
    return 0;

  return (psnip_cpuinfo[(i * 4) + r] >> b) & 1;
#elif defined(PSNIP_CPU_ARCH_ARM) || defined(PSNIP_CPU_ARCH_ARM_64)
    b = 1 << ((feature & 0xff) - 1);
  i = psnip_cpuinfo[(feature >> 0x08) & 0xff];
  return (psnip_cpuinfo[(feature >> 0x08) & 0xff] & b) == b;
#endif
}

int
psnip_cpu_feature_check_many (enum PSnipCPUFeature* feature) {
    int n;

    for (n = 0 ; feature[n] != PSNIP_CPU_FEATURE_NONE ; n++)
        if (!psnip_cpu_feature_check(feature[n]))
            return 0;

    return 1;
}

int
psnip_cpu_count (void) {
    static int count = 0;
    int c;

#if defined(_WIN32)
    DWORD_PTR lpProcessAffinityMask;
  DWORD_PTR lpSystemAffinityMask;
  int i;
#elif defined(PSNIP_CPU__IMPL_SYSCONF) && defined(HW_NCPU)
    int mib[2];
  size_t len;
#endif

    if (count != 0)
        return count;

#if defined(_WIN32)
    if (!GetProcessAffinityMask(GetCurrentProcess(), &lpProcessAffinityMask, &lpSystemAffinityMask)) {
    c = -1;
  } else {
    c = 0;
    for (i = 0 ; lpProcessAffinityMask != 0 ; lpProcessAffinityMask >>= 1)
      c += lpProcessAffinityMask & 1;
  }
#elif defined(_SC_NPROCESSORS_ONLN)
    c = sysconf (_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROC_ONLN)
    c = sysconf (_SC_NPROC_ONLN);
#elif defined(_hpux)
    c = mpctl(MPC_GETNUMSPUS, NULL, NULL);
#elif defined(HW_NCPU)
    c = 0;
  mib[0] = CTL_HW;
  mib[1] = HW_NCPU;
  len = sizeof(c);
  sysctl (mib, 2, &c, &len, NULL, 0);
#endif

    count = (c > 0) ? c : -1;

    return count;
}