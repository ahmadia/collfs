#!/usr/bin/env bash
#
# @ job_name            = kslrun_job
# @ job_type            = bluegene
# @ output              = ./$(job_name)_$(jobid).out
# @ error               = ./$(job_name)_$(jobid).err
# @ environment         = COPY_ALL; 
# @ wall_clock_limit    = 2:00:00,2:00:00
# @ notification        = always
# @ bg_size             = 128
# @ account_no          = k01

# @ queue

llx