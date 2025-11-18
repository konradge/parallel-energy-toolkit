// msr_reader.h

#ifndef MSR_READER_H
#define MSR_READER_H

#ifdef __cplusplus
extern "C" {
#endif

double read_intel_msr(int core);

#ifdef __cplusplus
}
#endif

#endif // MSR_READER_H