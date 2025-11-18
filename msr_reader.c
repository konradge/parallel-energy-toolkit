/**
 * \file msr_reader.c
 * Authors: Maximilian Krebs, Konrad Geller
 * \brief 
 * \version 0.1
 * \date 2025-10-21
 * 
 * Copyright 2025 SSE
 * 
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <math.h>
#include "msr_reader.h"

/**
 * \brief Read the given msr register file and read at the given offset
 * 
 * \param registerpath Path of the msr file in linux
 * \param offset Offset defining the register that should be read
 * \return uint64_t Returns read msr value
 */
uint64_t read_msr(const char *registerpath, uint32_t offset) {
    // Open the file once
    int fd = open(registerpath, O_RDONLY);

    // Check validity of file descriptor
    if (fd < 0) {
        perror("Error opening MSR file");
        return 0;
    }

    // Init the value and seek the register offset
    uint64_t value = 0;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("Error seeking MSR register");
        close(fd);
        return 0;
    }

    // Read the register value
    if (read(fd, &value, sizeof(value)) != sizeof(value)) {
        perror("Error reading MSR register");
        close(fd);
        return 0;
    }

    close(fd);
    return value;
}

/**
 * \brief Read the given energy and unit register under the defined path and convert them to joule
 * 
 * \param energyreg Register offset of the energy value
 * \param unitreg Register offset of the unit
 * \param registerpath Path of the msr register file
 * \return double Energy in Joule
 */
double get_register_values(uint32_t energyreg, uint32_t unitreg, const char *registerpath) {
  uint64_t energy = read_msr(registerpath, energyreg);
  uint64_t unit = read_msr(registerpath, unitreg);

  uint32_t cleaned_unit = (unit >> 8) & 0x1F;
  double energy_value = energy * pow(0.5, cleaned_unit);

  return energy_value;
}

/**
 * \brief Python method to read energy registers of the given msr register file on an INTEL CPU
 * 
 * \param self Python object
 * \param args Python arguments
 * \return PyObject* Python double object with the read energy
 */
double read_intel_msr(int core) {
    char registerpath[64];
    snprintf(registerpath, sizeof(registerpath), "/dev/cpu/%d/msr", core);
    uint32_t energyreg = 0x639;
    uint32_t unitreg = 0x606;

    double read_val = get_register_values(energyreg, unitreg, registerpath);
    return read_val;
}

/**
 * \brief Python method to read energy registers of the given msr register file on an AMD CPU
 * 
 * \param self Python object
 * \param args Python arguments
 * \return PyObject* Python double object with the read energy
 */
double read_amd_msr(int core) {
    char registerpath[64];
    snprintf(registerpath, sizeof(registerpath), "/dev/cpu/%d/msr", core);
    uint32_t energyreg = 0xC001029A;
    uint32_t unitreg = 0xC0010299;

    double read_val = get_register_values(energyreg, unitreg, registerpath);
    return read_val;
}