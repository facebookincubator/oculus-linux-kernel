# Android.mk to help quickly re-build the Kernel
#
# This makefile doesn't define any Android modules. It adds rules to help
# regenerate a Kernel (and boot.img) by running 'mm bootimage' in this directory
#

LOCAL_PATH := $(call my-dir)
