/*
 * Copyright (C) 2019  SUSE Software Solutions Germany GmbH
 *
 * This file is part of klp-ccp.
 *
 * klp-ccp is free software: you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * klp-ccp is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with klp-ccp. If not, see <https://www.gnu.org/licenses/>.
 */

{
  "m128bit-long-double",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "m32",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "m3dnow",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "m3dnowa",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "m64",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "m80387",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "m8bit-idiv",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "m96bit-long-double",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mabi=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mabm",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "maccumulate-outgoing-args",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "maddress-mode=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "madx",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "maes",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "malign-double",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "malign-functions=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "malign-jumps=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "malign-loops=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "malign-stringops",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "march=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "masm=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mavx",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mavx2",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mavx256-split-unaligned-load",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mavx256-split-unaligned-store",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mbmi",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mbmi2",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mbranch-cost=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mcld",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mcmodel=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mcpu=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
  .alias = { "mtune=" },
},
{
  "mcrc32",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mcx16",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mdispatch-scheduler",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mf16c",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mfancy-math-387",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mfentry",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mfma",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mfma4",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mforce-drap",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mfp-ret-in-387",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mfpmath=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mfsgsbase",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mfxsr",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mhard-float",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mhle",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mieee-fp",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mincoming-stack-boundary=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "minline-all-stringops",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "minline-stringops-dynamically",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mintel-syntax",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = false,
  .alias = { "masm=", "intel", "att" },
},
{
  "mlarge-data-threshold=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mlong-double-64",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mlong-double-80",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mlwp",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mlzcnt",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mmmx",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mmovbe",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mms-bitfields",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mno-align-stringops",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mno-fancy-math-387",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mno-push-args",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mno-red-zone",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mno-sse4",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "momit-leaf-frame-pointer",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mpc32",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mpc64",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mpc80",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mpclmul",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mpopcnt",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mprefer-avx128",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mpreferred-stack-boundary=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mprfchw",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mpush-args",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mrdrnd",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mrdseed",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mrecip",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mrecip=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mred-zone",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mregparm=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mrtd",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mrtm",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msahf",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msoft-float",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse2",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse2avx",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse3",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse4",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "msse4.1",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse4.2",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse4a",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "msse5",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = false,
  .alias = { "mavx" },
},
{
  "msseregparm",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mssse3",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mstack-arg-probe",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mstackrealign",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mstringop-strategy=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mtbm",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mtls-dialect=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mtls-direct-seg-refs",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mtune=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mveclibabi=",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_joined,
  .reject_negative = true,
},
{
  "mvect8-ret-in-mem",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mvzeroupper",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mx32",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
  .reject_negative = true,
},
{
  "mxop",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mxsave",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
{
  "mxsaveopt",
  gcc_cmdline_parser::option::comp_target,
  gcc_cmdline_parser::option::arg_none,
},
