// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"

#define FUNCTION(fname)			                \
	[msm_mux_##fname] = {		                \
		.name = #fname,				\
		.groups = fname##_groups,               \
		.ngroups = ARRAY_SIZE(fname##_groups),	\
	}

#define REG_BASE 0x100000
#define REG_SIZE 0x1000
#define PINGROUP(id, f1, f2, f3, f4, f5, f6, f7, f8, f9, wake_off, bit)	\
	{					        \
		.name = "gpio" #id,			\
		.pins = gpio##id##_pins,		\
		.npins = (unsigned int)ARRAY_SIZE(gpio##id##_pins),	\
		.funcs = (int[]){			\
			msm_mux_gpio, /* gpio mode */	\
			msm_mux_##f1,			\
			msm_mux_##f2,			\
			msm_mux_##f3,			\
			msm_mux_##f4,			\
			msm_mux_##f5,			\
			msm_mux_##f6,			\
			msm_mux_##f7,			\
			msm_mux_##f8,			\
			msm_mux_##f9			\
		},				        \
		.nfuncs = 10,				\
		.ctl_reg = REG_BASE + REG_SIZE * id,			\
		.io_reg = REG_BASE + 0x4 + REG_SIZE * id,		\
		.intr_cfg_reg = REG_BASE + 0x8 + REG_SIZE * id,		\
		.intr_status_reg = REG_BASE + 0xc + REG_SIZE * id,	\
		.intr_target_reg = REG_BASE + 0x8 + REG_SIZE * id,	\
		.mux_bit = 2,			\
		.pull_bit = 0,			\
		.drv_bit = 6,			\
		.egpio_enable = 12,		\
		.egpio_present = 11,		\
		.oe_bit = 9,			\
		.in_bit = 0,			\
		.out_bit = 1,			\
		.intr_enable_bit = 0,		\
		.intr_status_bit = 0,		\
		.intr_target_bit = 5,		\
		.intr_target_kpss_val = 3,	\
		.intr_raw_status_bit = 4,	\
		.intr_polarity_bit = 1,		\
		.intr_detection_bit = 2,	\
		.intr_detection_width = 2,	\
		.wake_reg = REG_BASE + wake_off,	\
		.wake_bit = bit,		\
	}

#define SDC_QDSD_PINGROUP(pg_name, ctl, pull, drv)	\
	{					        \
		.name = #pg_name,			\
		.pins = pg_name##_pins,			\
		.npins = (unsigned int)ARRAY_SIZE(pg_name##_pins),	\
		.ctl_reg = ctl,				\
		.io_reg = 0,				\
		.intr_cfg_reg = 0,			\
		.intr_status_reg = 0,			\
		.intr_target_reg = 0,			\
		.mux_bit = -1,				\
		.pull_bit = pull,			\
		.drv_bit = drv,				\
		.oe_bit = -1,				\
		.in_bit = -1,				\
		.out_bit = -1,				\
		.intr_enable_bit = -1,			\
		.intr_status_bit = -1,			\
		.intr_target_bit = -1,			\
		.intr_raw_status_bit = -1,		\
		.intr_polarity_bit = -1,		\
		.intr_detection_bit = -1,		\
		.intr_detection_width = -1,		\
	}

#define UFS_RESET(pg_name, offset)				\
	{					        \
		.name = #pg_name,			\
		.pins = pg_name##_pins,			\
		.npins = (unsigned int)ARRAY_SIZE(pg_name##_pins),	\
		.ctl_reg = offset,			\
		.io_reg = offset + 0x4,			\
		.intr_cfg_reg = 0,			\
		.intr_status_reg = 0,			\
		.intr_target_reg = 0,			\
		.mux_bit = -1,				\
		.pull_bit = 3,				\
		.drv_bit = 0,				\
		.oe_bit = -1,				\
		.in_bit = -1,				\
		.out_bit = 0,				\
		.intr_enable_bit = -1,			\
		.intr_status_bit = -1,			\
		.intr_target_bit = -1,			\
		.intr_raw_status_bit = -1,		\
		.intr_polarity_bit = -1,		\
		.intr_detection_bit = -1,		\
		.intr_detection_width = -1,		\
		.wake_reg = 0,				\
		.wake_bit = -1,				\
	}

#define QUP_I3C(qup_mode, qup_offset)                  \
	{                                               \
		.mode = qup_mode,                       \
		.offset = qup_offset,                   \
	}


#define QUP_I3C_0_MODE_OFFSET	0xE5000
#define QUP_I3C_1_MODE_OFFSET	0xE6000
#define QUP_I3C_8_MODE_OFFSET	0xE7000
#define QUP_I3C_9_MODE_OFFSET	0xE8000

static const struct pinctrl_pin_desc shima_pins[] = {
	PINCTRL_PIN(0, "GPIO_0"),
	PINCTRL_PIN(1, "GPIO_1"),
	PINCTRL_PIN(2, "GPIO_2"),
	PINCTRL_PIN(3, "GPIO_3"),
	PINCTRL_PIN(4, "GPIO_4"),
	PINCTRL_PIN(5, "GPIO_5"),
	PINCTRL_PIN(6, "GPIO_6"),
	PINCTRL_PIN(7, "GPIO_7"),
	PINCTRL_PIN(8, "GPIO_8"),
	PINCTRL_PIN(9, "GPIO_9"),
	PINCTRL_PIN(10, "GPIO_10"),
	PINCTRL_PIN(11, "GPIO_11"),
	PINCTRL_PIN(12, "GPIO_12"),
	PINCTRL_PIN(13, "GPIO_13"),
	PINCTRL_PIN(14, "GPIO_14"),
	PINCTRL_PIN(15, "GPIO_15"),
	PINCTRL_PIN(16, "GPIO_16"),
	PINCTRL_PIN(17, "GPIO_17"),
	PINCTRL_PIN(18, "GPIO_18"),
	PINCTRL_PIN(19, "GPIO_19"),
	PINCTRL_PIN(20, "GPIO_20"),
	PINCTRL_PIN(21, "GPIO_21"),
	PINCTRL_PIN(22, "GPIO_22"),
	PINCTRL_PIN(23, "GPIO_23"),
	PINCTRL_PIN(24, "GPIO_24"),
	PINCTRL_PIN(25, "GPIO_25"),
	PINCTRL_PIN(26, "GPIO_26"),
	PINCTRL_PIN(27, "GPIO_27"),
	PINCTRL_PIN(28, "GPIO_28"),
	PINCTRL_PIN(29, "GPIO_29"),
	PINCTRL_PIN(30, "GPIO_30"),
	PINCTRL_PIN(31, "GPIO_31"),
	PINCTRL_PIN(32, "GPIO_32"),
	PINCTRL_PIN(33, "GPIO_33"),
	PINCTRL_PIN(34, "GPIO_34"),
	PINCTRL_PIN(35, "GPIO_35"),
	PINCTRL_PIN(36, "GPIO_36"),
	PINCTRL_PIN(37, "GPIO_37"),
	PINCTRL_PIN(38, "GPIO_38"),
	PINCTRL_PIN(39, "GPIO_39"),
	PINCTRL_PIN(40, "GPIO_40"),
	PINCTRL_PIN(41, "GPIO_41"),
	PINCTRL_PIN(42, "GPIO_42"),
	PINCTRL_PIN(43, "GPIO_43"),
	PINCTRL_PIN(44, "GPIO_44"),
	PINCTRL_PIN(45, "GPIO_45"),
	PINCTRL_PIN(46, "GPIO_46"),
	PINCTRL_PIN(47, "GPIO_47"),
	PINCTRL_PIN(48, "GPIO_48"),
	PINCTRL_PIN(49, "GPIO_49"),
	PINCTRL_PIN(50, "GPIO_50"),
	PINCTRL_PIN(51, "GPIO_51"),
	PINCTRL_PIN(52, "GPIO_52"),
	PINCTRL_PIN(53, "GPIO_53"),
	PINCTRL_PIN(54, "GPIO_54"),
	PINCTRL_PIN(55, "GPIO_55"),
	PINCTRL_PIN(56, "GPIO_56"),
	PINCTRL_PIN(57, "GPIO_57"),
	PINCTRL_PIN(58, "GPIO_58"),
	PINCTRL_PIN(59, "GPIO_59"),
	PINCTRL_PIN(60, "GPIO_60"),
	PINCTRL_PIN(61, "GPIO_61"),
	PINCTRL_PIN(62, "GPIO_62"),
	PINCTRL_PIN(63, "GPIO_63"),
	PINCTRL_PIN(64, "GPIO_64"),
	PINCTRL_PIN(65, "GPIO_65"),
	PINCTRL_PIN(66, "GPIO_66"),
	PINCTRL_PIN(67, "GPIO_67"),
	PINCTRL_PIN(68, "GPIO_68"),
	PINCTRL_PIN(69, "GPIO_69"),
	PINCTRL_PIN(70, "GPIO_70"),
	PINCTRL_PIN(71, "GPIO_71"),
	PINCTRL_PIN(72, "GPIO_72"),
	PINCTRL_PIN(73, "GPIO_73"),
	PINCTRL_PIN(74, "GPIO_74"),
	PINCTRL_PIN(75, "GPIO_75"),
	PINCTRL_PIN(76, "GPIO_76"),
	PINCTRL_PIN(77, "GPIO_77"),
	PINCTRL_PIN(78, "GPIO_78"),
	PINCTRL_PIN(79, "GPIO_79"),
	PINCTRL_PIN(80, "GPIO_80"),
	PINCTRL_PIN(81, "GPIO_81"),
	PINCTRL_PIN(82, "GPIO_82"),
	PINCTRL_PIN(83, "GPIO_83"),
	PINCTRL_PIN(84, "GPIO_84"),
	PINCTRL_PIN(85, "GPIO_85"),
	PINCTRL_PIN(86, "GPIO_86"),
	PINCTRL_PIN(87, "GPIO_87"),
	PINCTRL_PIN(88, "GPIO_88"),
	PINCTRL_PIN(89, "GPIO_89"),
	PINCTRL_PIN(90, "GPIO_90"),
	PINCTRL_PIN(91, "GPIO_91"),
	PINCTRL_PIN(92, "GPIO_92"),
	PINCTRL_PIN(93, "GPIO_93"),
	PINCTRL_PIN(94, "GPIO_94"),
	PINCTRL_PIN(95, "GPIO_95"),
	PINCTRL_PIN(96, "GPIO_96"),
	PINCTRL_PIN(97, "GPIO_97"),
	PINCTRL_PIN(98, "GPIO_98"),
	PINCTRL_PIN(99, "GPIO_99"),
	PINCTRL_PIN(100, "GPIO_100"),
	PINCTRL_PIN(101, "GPIO_101"),
	PINCTRL_PIN(102, "GPIO_102"),
	PINCTRL_PIN(103, "GPIO_103"),
	PINCTRL_PIN(104, "GPIO_104"),
	PINCTRL_PIN(105, "GPIO_105"),
	PINCTRL_PIN(106, "GPIO_106"),
	PINCTRL_PIN(107, "GPIO_107"),
	PINCTRL_PIN(108, "GPIO_108"),
	PINCTRL_PIN(109, "GPIO_109"),
	PINCTRL_PIN(110, "GPIO_110"),
	PINCTRL_PIN(111, "GPIO_111"),
	PINCTRL_PIN(112, "GPIO_112"),
	PINCTRL_PIN(113, "GPIO_113"),
	PINCTRL_PIN(114, "GPIO_114"),
	PINCTRL_PIN(115, "GPIO_115"),
	PINCTRL_PIN(116, "GPIO_116"),
	PINCTRL_PIN(117, "GPIO_117"),
	PINCTRL_PIN(118, "GPIO_118"),
	PINCTRL_PIN(119, "GPIO_119"),
	PINCTRL_PIN(120, "GPIO_120"),
	PINCTRL_PIN(121, "GPIO_121"),
	PINCTRL_PIN(122, "GPIO_122"),
	PINCTRL_PIN(123, "GPIO_123"),
	PINCTRL_PIN(124, "GPIO_124"),
	PINCTRL_PIN(125, "GPIO_125"),
	PINCTRL_PIN(126, "GPIO_126"),
	PINCTRL_PIN(127, "GPIO_127"),
	PINCTRL_PIN(128, "GPIO_128"),
	PINCTRL_PIN(129, "GPIO_129"),
	PINCTRL_PIN(130, "GPIO_130"),
	PINCTRL_PIN(131, "GPIO_131"),
	PINCTRL_PIN(132, "GPIO_132"),
	PINCTRL_PIN(133, "GPIO_133"),
	PINCTRL_PIN(134, "GPIO_134"),
	PINCTRL_PIN(135, "GPIO_135"),
	PINCTRL_PIN(136, "GPIO_136"),
	PINCTRL_PIN(137, "GPIO_137"),
	PINCTRL_PIN(138, "GPIO_138"),
	PINCTRL_PIN(139, "GPIO_139"),
	PINCTRL_PIN(140, "GPIO_140"),
	PINCTRL_PIN(141, "GPIO_141"),
	PINCTRL_PIN(142, "GPIO_142"),
	PINCTRL_PIN(143, "GPIO_143"),
	PINCTRL_PIN(144, "GPIO_144"),
	PINCTRL_PIN(145, "GPIO_145"),
	PINCTRL_PIN(146, "GPIO_146"),
	PINCTRL_PIN(147, "GPIO_147"),
	PINCTRL_PIN(148, "GPIO_148"),
	PINCTRL_PIN(149, "GPIO_149"),
	PINCTRL_PIN(150, "GPIO_150"),
	PINCTRL_PIN(151, "GPIO_151"),
	PINCTRL_PIN(152, "GPIO_152"),
	PINCTRL_PIN(153, "GPIO_153"),
	PINCTRL_PIN(154, "GPIO_154"),
	PINCTRL_PIN(155, "GPIO_155"),
	PINCTRL_PIN(156, "GPIO_156"),
	PINCTRL_PIN(157, "GPIO_157"),
	PINCTRL_PIN(158, "GPIO_158"),
	PINCTRL_PIN(159, "GPIO_159"),
	PINCTRL_PIN(160, "GPIO_160"),
	PINCTRL_PIN(161, "GPIO_161"),
	PINCTRL_PIN(162, "GPIO_162"),
	PINCTRL_PIN(163, "GPIO_163"),
	PINCTRL_PIN(164, "GPIO_164"),
	PINCTRL_PIN(165, "GPIO_165"),
	PINCTRL_PIN(166, "GPIO_166"),
	PINCTRL_PIN(167, "GPIO_167"),
	PINCTRL_PIN(168, "GPIO_168"),
	PINCTRL_PIN(169, "GPIO_169"),
	PINCTRL_PIN(170, "GPIO_170"),
	PINCTRL_PIN(171, "GPIO_171"),
	PINCTRL_PIN(172, "GPIO_172"),
	PINCTRL_PIN(173, "GPIO_173"),
	PINCTRL_PIN(174, "GPIO_174"),
	PINCTRL_PIN(175, "GPIO_175"),
	PINCTRL_PIN(176, "GPIO_176"),
	PINCTRL_PIN(177, "GPIO_177"),
	PINCTRL_PIN(178, "GPIO_178"),
	PINCTRL_PIN(179, "GPIO_179"),
	PINCTRL_PIN(180, "GPIO_180"),
	PINCTRL_PIN(181, "GPIO_181"),
	PINCTRL_PIN(182, "GPIO_182"),
	PINCTRL_PIN(183, "GPIO_183"),
	PINCTRL_PIN(184, "GPIO_184"),
	PINCTRL_PIN(185, "GPIO_185"),
	PINCTRL_PIN(186, "GPIO_186"),
	PINCTRL_PIN(187, "GPIO_187"),
	PINCTRL_PIN(188, "GPIO_188"),
	PINCTRL_PIN(189, "GPIO_189"),
	PINCTRL_PIN(190, "GPIO_190"),
	PINCTRL_PIN(191, "GPIO_191"),
	PINCTRL_PIN(192, "GPIO_192"),
	PINCTRL_PIN(193, "GPIO_193"),
	PINCTRL_PIN(194, "GPIO_194"),
	PINCTRL_PIN(195, "GPIO_195"),
	PINCTRL_PIN(196, "GPIO_196"),
	PINCTRL_PIN(197, "GPIO_197"),
	PINCTRL_PIN(198, "GPIO_198"),
	PINCTRL_PIN(199, "GPIO_199"),
	PINCTRL_PIN(200, "GPIO_200"),
	PINCTRL_PIN(201, "GPIO_201"),
	PINCTRL_PIN(202, "GPIO_202"),
	PINCTRL_PIN(203, "GPIO_203"),
	PINCTRL_PIN(204, "UFS_RESET"),
	PINCTRL_PIN(205, "SDC1_RCLK"),
	PINCTRL_PIN(206, "SDC1_CLK"),
	PINCTRL_PIN(207, "SDC1_CMD"),
	PINCTRL_PIN(208, "SDC1_DATA"),
	PINCTRL_PIN(209, "SDC2_CLK"),
	PINCTRL_PIN(210, "SDC2_CMD"),
	PINCTRL_PIN(211, "SDC2_DATA"),

};

#define DECLARE_MSM_GPIO_PINS(pin) \
	static const unsigned int gpio##pin##_pins[] = { pin }
DECLARE_MSM_GPIO_PINS(0);
DECLARE_MSM_GPIO_PINS(1);
DECLARE_MSM_GPIO_PINS(2);
DECLARE_MSM_GPIO_PINS(3);
DECLARE_MSM_GPIO_PINS(4);
DECLARE_MSM_GPIO_PINS(5);
DECLARE_MSM_GPIO_PINS(6);
DECLARE_MSM_GPIO_PINS(7);
DECLARE_MSM_GPIO_PINS(8);
DECLARE_MSM_GPIO_PINS(9);
DECLARE_MSM_GPIO_PINS(10);
DECLARE_MSM_GPIO_PINS(11);
DECLARE_MSM_GPIO_PINS(12);
DECLARE_MSM_GPIO_PINS(13);
DECLARE_MSM_GPIO_PINS(14);
DECLARE_MSM_GPIO_PINS(15);
DECLARE_MSM_GPIO_PINS(16);
DECLARE_MSM_GPIO_PINS(17);
DECLARE_MSM_GPIO_PINS(18);
DECLARE_MSM_GPIO_PINS(19);
DECLARE_MSM_GPIO_PINS(20);
DECLARE_MSM_GPIO_PINS(21);
DECLARE_MSM_GPIO_PINS(22);
DECLARE_MSM_GPIO_PINS(23);
DECLARE_MSM_GPIO_PINS(24);
DECLARE_MSM_GPIO_PINS(25);
DECLARE_MSM_GPIO_PINS(26);
DECLARE_MSM_GPIO_PINS(27);
DECLARE_MSM_GPIO_PINS(28);
DECLARE_MSM_GPIO_PINS(29);
DECLARE_MSM_GPIO_PINS(30);
DECLARE_MSM_GPIO_PINS(31);
DECLARE_MSM_GPIO_PINS(32);
DECLARE_MSM_GPIO_PINS(33);
DECLARE_MSM_GPIO_PINS(34);
DECLARE_MSM_GPIO_PINS(35);
DECLARE_MSM_GPIO_PINS(36);
DECLARE_MSM_GPIO_PINS(37);
DECLARE_MSM_GPIO_PINS(38);
DECLARE_MSM_GPIO_PINS(39);
DECLARE_MSM_GPIO_PINS(40);
DECLARE_MSM_GPIO_PINS(41);
DECLARE_MSM_GPIO_PINS(42);
DECLARE_MSM_GPIO_PINS(43);
DECLARE_MSM_GPIO_PINS(44);
DECLARE_MSM_GPIO_PINS(45);
DECLARE_MSM_GPIO_PINS(46);
DECLARE_MSM_GPIO_PINS(47);
DECLARE_MSM_GPIO_PINS(48);
DECLARE_MSM_GPIO_PINS(49);
DECLARE_MSM_GPIO_PINS(50);
DECLARE_MSM_GPIO_PINS(51);
DECLARE_MSM_GPIO_PINS(52);
DECLARE_MSM_GPIO_PINS(53);
DECLARE_MSM_GPIO_PINS(54);
DECLARE_MSM_GPIO_PINS(55);
DECLARE_MSM_GPIO_PINS(56);
DECLARE_MSM_GPIO_PINS(57);
DECLARE_MSM_GPIO_PINS(58);
DECLARE_MSM_GPIO_PINS(59);
DECLARE_MSM_GPIO_PINS(60);
DECLARE_MSM_GPIO_PINS(61);
DECLARE_MSM_GPIO_PINS(62);
DECLARE_MSM_GPIO_PINS(63);
DECLARE_MSM_GPIO_PINS(64);
DECLARE_MSM_GPIO_PINS(65);
DECLARE_MSM_GPIO_PINS(66);
DECLARE_MSM_GPIO_PINS(67);
DECLARE_MSM_GPIO_PINS(68);
DECLARE_MSM_GPIO_PINS(69);
DECLARE_MSM_GPIO_PINS(70);
DECLARE_MSM_GPIO_PINS(71);
DECLARE_MSM_GPIO_PINS(72);
DECLARE_MSM_GPIO_PINS(73);
DECLARE_MSM_GPIO_PINS(74);
DECLARE_MSM_GPIO_PINS(75);
DECLARE_MSM_GPIO_PINS(76);
DECLARE_MSM_GPIO_PINS(77);
DECLARE_MSM_GPIO_PINS(78);
DECLARE_MSM_GPIO_PINS(79);
DECLARE_MSM_GPIO_PINS(80);
DECLARE_MSM_GPIO_PINS(81);
DECLARE_MSM_GPIO_PINS(82);
DECLARE_MSM_GPIO_PINS(83);
DECLARE_MSM_GPIO_PINS(84);
DECLARE_MSM_GPIO_PINS(85);
DECLARE_MSM_GPIO_PINS(86);
DECLARE_MSM_GPIO_PINS(87);
DECLARE_MSM_GPIO_PINS(88);
DECLARE_MSM_GPIO_PINS(89);
DECLARE_MSM_GPIO_PINS(90);
DECLARE_MSM_GPIO_PINS(91);
DECLARE_MSM_GPIO_PINS(92);
DECLARE_MSM_GPIO_PINS(93);
DECLARE_MSM_GPIO_PINS(94);
DECLARE_MSM_GPIO_PINS(95);
DECLARE_MSM_GPIO_PINS(96);
DECLARE_MSM_GPIO_PINS(97);
DECLARE_MSM_GPIO_PINS(98);
DECLARE_MSM_GPIO_PINS(99);
DECLARE_MSM_GPIO_PINS(100);
DECLARE_MSM_GPIO_PINS(101);
DECLARE_MSM_GPIO_PINS(102);
DECLARE_MSM_GPIO_PINS(103);
DECLARE_MSM_GPIO_PINS(104);
DECLARE_MSM_GPIO_PINS(105);
DECLARE_MSM_GPIO_PINS(106);
DECLARE_MSM_GPIO_PINS(107);
DECLARE_MSM_GPIO_PINS(108);
DECLARE_MSM_GPIO_PINS(109);
DECLARE_MSM_GPIO_PINS(110);
DECLARE_MSM_GPIO_PINS(111);
DECLARE_MSM_GPIO_PINS(112);
DECLARE_MSM_GPIO_PINS(113);
DECLARE_MSM_GPIO_PINS(114);
DECLARE_MSM_GPIO_PINS(115);
DECLARE_MSM_GPIO_PINS(116);
DECLARE_MSM_GPIO_PINS(117);
DECLARE_MSM_GPIO_PINS(118);
DECLARE_MSM_GPIO_PINS(119);
DECLARE_MSM_GPIO_PINS(120);
DECLARE_MSM_GPIO_PINS(121);
DECLARE_MSM_GPIO_PINS(122);
DECLARE_MSM_GPIO_PINS(123);
DECLARE_MSM_GPIO_PINS(124);
DECLARE_MSM_GPIO_PINS(125);
DECLARE_MSM_GPIO_PINS(126);
DECLARE_MSM_GPIO_PINS(127);
DECLARE_MSM_GPIO_PINS(128);
DECLARE_MSM_GPIO_PINS(129);
DECLARE_MSM_GPIO_PINS(130);
DECLARE_MSM_GPIO_PINS(131);
DECLARE_MSM_GPIO_PINS(132);
DECLARE_MSM_GPIO_PINS(133);
DECLARE_MSM_GPIO_PINS(134);
DECLARE_MSM_GPIO_PINS(135);
DECLARE_MSM_GPIO_PINS(136);
DECLARE_MSM_GPIO_PINS(137);
DECLARE_MSM_GPIO_PINS(138);
DECLARE_MSM_GPIO_PINS(139);
DECLARE_MSM_GPIO_PINS(140);
DECLARE_MSM_GPIO_PINS(141);
DECLARE_MSM_GPIO_PINS(142);
DECLARE_MSM_GPIO_PINS(143);
DECLARE_MSM_GPIO_PINS(144);
DECLARE_MSM_GPIO_PINS(145);
DECLARE_MSM_GPIO_PINS(146);
DECLARE_MSM_GPIO_PINS(147);
DECLARE_MSM_GPIO_PINS(148);
DECLARE_MSM_GPIO_PINS(149);
DECLARE_MSM_GPIO_PINS(150);
DECLARE_MSM_GPIO_PINS(151);
DECLARE_MSM_GPIO_PINS(152);
DECLARE_MSM_GPIO_PINS(153);
DECLARE_MSM_GPIO_PINS(154);
DECLARE_MSM_GPIO_PINS(155);
DECLARE_MSM_GPIO_PINS(156);
DECLARE_MSM_GPIO_PINS(157);
DECLARE_MSM_GPIO_PINS(158);
DECLARE_MSM_GPIO_PINS(159);
DECLARE_MSM_GPIO_PINS(160);
DECLARE_MSM_GPIO_PINS(161);
DECLARE_MSM_GPIO_PINS(162);
DECLARE_MSM_GPIO_PINS(163);
DECLARE_MSM_GPIO_PINS(164);
DECLARE_MSM_GPIO_PINS(165);
DECLARE_MSM_GPIO_PINS(166);
DECLARE_MSM_GPIO_PINS(167);
DECLARE_MSM_GPIO_PINS(168);
DECLARE_MSM_GPIO_PINS(169);
DECLARE_MSM_GPIO_PINS(170);
DECLARE_MSM_GPIO_PINS(171);
DECLARE_MSM_GPIO_PINS(172);
DECLARE_MSM_GPIO_PINS(173);
DECLARE_MSM_GPIO_PINS(174);
DECLARE_MSM_GPIO_PINS(175);
DECLARE_MSM_GPIO_PINS(176);
DECLARE_MSM_GPIO_PINS(177);
DECLARE_MSM_GPIO_PINS(178);
DECLARE_MSM_GPIO_PINS(179);
DECLARE_MSM_GPIO_PINS(180);
DECLARE_MSM_GPIO_PINS(181);
DECLARE_MSM_GPIO_PINS(182);
DECLARE_MSM_GPIO_PINS(183);
DECLARE_MSM_GPIO_PINS(184);
DECLARE_MSM_GPIO_PINS(185);
DECLARE_MSM_GPIO_PINS(186);
DECLARE_MSM_GPIO_PINS(187);
DECLARE_MSM_GPIO_PINS(188);
DECLARE_MSM_GPIO_PINS(189);
DECLARE_MSM_GPIO_PINS(190);
DECLARE_MSM_GPIO_PINS(191);
DECLARE_MSM_GPIO_PINS(192);
DECLARE_MSM_GPIO_PINS(193);
DECLARE_MSM_GPIO_PINS(194);
DECLARE_MSM_GPIO_PINS(195);
DECLARE_MSM_GPIO_PINS(196);
DECLARE_MSM_GPIO_PINS(197);
DECLARE_MSM_GPIO_PINS(198);
DECLARE_MSM_GPIO_PINS(199);
DECLARE_MSM_GPIO_PINS(200);
DECLARE_MSM_GPIO_PINS(201);
DECLARE_MSM_GPIO_PINS(202);
DECLARE_MSM_GPIO_PINS(203);

static const unsigned int sdc1_rclk_pins[] = { 205 };
static const unsigned int sdc1_clk_pins[] = { 206 };
static const unsigned int sdc1_cmd_pins[] = { 207 };
static const unsigned int sdc1_data_pins[] = { 208 };
static const unsigned int sdc2_clk_pins[] = { 209 };
static const unsigned int sdc2_cmd_pins[] = { 210};
static const unsigned int sdc2_data_pins[] = { 211 };
static const unsigned int ufs_reset_pins[] = { 204 };

enum shima_functions {
	msm_mux_gpio,
	msm_mux_atest_char,
	msm_mux_atest_char0,
	msm_mux_atest_char1,
	msm_mux_atest_char2,
	msm_mux_atest_char3,
	msm_mux_atest_usb0,
	msm_mux_atest_usb00,
	msm_mux_atest_usb01,
	msm_mux_atest_usb02,
	msm_mux_atest_usb03,
	msm_mux_audio_ref,
	msm_mux_cam_mclk,
	msm_mux_cci_async,
	msm_mux_cci_i2c,
	msm_mux_cci_timer0,
	msm_mux_cci_timer1,
	msm_mux_cci_timer2,
	msm_mux_cci_timer3,
	msm_mux_cci_timer4,
	msm_mux_cmu_rng0,
	msm_mux_cmu_rng1,
	msm_mux_cmu_rng2,
	msm_mux_cmu_rng3,
	msm_mux_coex_uart1,
	msm_mux_cri_trng,
	msm_mux_cri_trng0,
	msm_mux_cri_trng1,
	msm_mux_dbg_out,
	msm_mux_ddr_bist,
	msm_mux_ddr_pxi0,
	msm_mux_ddr_pxi1,
	msm_mux_dp_hot,
	msm_mux_dp_lcd,
	msm_mux_gcc_gp1,
	msm_mux_gcc_gp2,
	msm_mux_gcc_gp3,
	msm_mux_host2wlan_sol,
	msm_mux_ibi_i3c,
	msm_mux_jitter_bist,
	msm_mux_lpass_slimbus,
	msm_mux_mdp_vsync,
	msm_mux_mdp_vsync0,
	msm_mux_mdp_vsync1,
	msm_mux_mdp_vsync2,
	msm_mux_mdp_vsync3,
	msm_mux_mi2s0_data0,
	msm_mux_mi2s0_data1,
	msm_mux_mi2s0_sck,
	msm_mux_mi2s0_ws,
	msm_mux_mi2s1_data0,
	msm_mux_mi2s1_data1,
	msm_mux_mi2s1_sck,
	msm_mux_mi2s1_ws,
	msm_mux_mi2s2_data0,
	msm_mux_mi2s2_data1,
	msm_mux_mi2s2_sck,
	msm_mux_mi2s2_ws,
	msm_mux_mss_grfc0,
	msm_mux_mss_grfc1,
	msm_mux_mss_grfc10,
	msm_mux_mss_grfc11,
	msm_mux_mss_grfc12,
	msm_mux_mss_grfc2,
	msm_mux_mss_grfc3,
	msm_mux_mss_grfc4,
	msm_mux_mss_grfc5,
	msm_mux_mss_grfc6,
	msm_mux_mss_grfc7,
	msm_mux_mss_grfc8,
	msm_mux_mss_grfc9,
	msm_mux_nav_gpio,
	msm_mux_pa_indicator,
	msm_mux_pcie0_clkreqn,
	msm_mux_pcie1_clkreqn,
	msm_mux_phase_flag0,
	msm_mux_phase_flag1,
	msm_mux_phase_flag10,
	msm_mux_phase_flag11,
	msm_mux_phase_flag12,
	msm_mux_phase_flag13,
	msm_mux_phase_flag14,
	msm_mux_phase_flag15,
	msm_mux_phase_flag16,
	msm_mux_phase_flag17,
	msm_mux_phase_flag18,
	msm_mux_phase_flag19,
	msm_mux_phase_flag2,
	msm_mux_phase_flag20,
	msm_mux_phase_flag21,
	msm_mux_phase_flag22,
	msm_mux_phase_flag23,
	msm_mux_phase_flag24,
	msm_mux_phase_flag25,
	msm_mux_phase_flag26,
	msm_mux_phase_flag27,
	msm_mux_phase_flag28,
	msm_mux_phase_flag29,
	msm_mux_phase_flag3,
	msm_mux_phase_flag30,
	msm_mux_phase_flag31,
	msm_mux_phase_flag4,
	msm_mux_phase_flag5,
	msm_mux_phase_flag6,
	msm_mux_phase_flag7,
	msm_mux_phase_flag8,
	msm_mux_phase_flag9,
	msm_mux_pll_bist,
	msm_mux_pll_clk,
	msm_mux_pri_mi2s,
	msm_mux_prng_rosc,
	msm_mux_qdss_cti,
	msm_mux_qdss_gpio,
	msm_mux_qdss_gpio0,
	msm_mux_qdss_gpio1,
	msm_mux_qdss_gpio10,
	msm_mux_qdss_gpio11,
	msm_mux_qdss_gpio12,
	msm_mux_qdss_gpio13,
	msm_mux_qdss_gpio14,
	msm_mux_qdss_gpio15,
	msm_mux_qdss_gpio2,
	msm_mux_qdss_gpio3,
	msm_mux_qdss_gpio4,
	msm_mux_qdss_gpio5,
	msm_mux_qdss_gpio6,
	msm_mux_qdss_gpio7,
	msm_mux_qdss_gpio8,
	msm_mux_qdss_gpio9,
	msm_mux_qlink0_enable,
	msm_mux_qlink0_request,
	msm_mux_qlink0_wmss,
	msm_mux_qlink1_enable,
	msm_mux_qlink1_request,
	msm_mux_qlink1_wmss,
	msm_mux_qspi_clk,
	msm_mux_qspi_cs,
	msm_mux_qspi_data,
	msm_mux_qup0,
	msm_mux_qup1,
	msm_mux_qup10,
	msm_mux_qup11,
	msm_mux_qup12,
	msm_mux_qup13,
	msm_mux_qup14,
	msm_mux_qup15,
	msm_mux_qup2,
	msm_mux_qup3,
	msm_mux_qup4,
	msm_mux_qup5,
	msm_mux_qup6,
	msm_mux_qup7,
	msm_mux_qup8,
	msm_mux_qup9,
	msm_mux_qup_l4,
	msm_mux_qup_l5,
	msm_mux_qup_l6,
	msm_mux_sd_write,
	msm_mux_sdc40,
	msm_mux_sdc41,
	msm_mux_sdc42,
	msm_mux_sdc43,
	msm_mux_sdc4_clk,
	msm_mux_sdc4_cmd,
	msm_mux_sec_mi2s,
	msm_mux_tb_trig,
	msm_mux_tgu_ch0,
	msm_mux_tgu_ch1,
	msm_mux_tsense_pwm1,
	msm_mux_tsense_pwm2,
	msm_mux_uim0_clk,
	msm_mux_uim0_data,
	msm_mux_uim0_present,
	msm_mux_uim0_reset,
	msm_mux_uim1_clk,
	msm_mux_uim1_data,
	msm_mux_uim1_present,
	msm_mux_uim1_reset,
	msm_mux_usb2phy_ac,
	msm_mux_usb_phy,
	msm_mux_vfr_0,
	msm_mux_vfr_1,
	msm_mux_vsense_trigger,
	msm_mux_NA,
};

static const char * const gpio_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3", "gpio4", "gpio5", "gpio6", "gpio7",
	"gpio8", "gpio9", "gpio10", "gpio11", "gpio12", "gpio13", "gpio14",
	"gpio15", "gpio16", "gpio17", "gpio18", "gpio19", "gpio20", "gpio21",
	"gpio22", "gpio23", "gpio24", "gpio25", "gpio26", "gpio27", "gpio28",
	"gpio29", "gpio30", "gpio31", "gpio32", "gpio33", "gpio34", "gpio35",
	"gpio36", "gpio37", "gpio38", "gpio39", "gpio40", "gpio41", "gpio42",
	"gpio43", "gpio44", "gpio45", "gpio46", "gpio47", "gpio48", "gpio49",
	"gpio50", "gpio51", "gpio52", "gpio53", "gpio54", "gpio55", "gpio56",
	"gpio57", "gpio58", "gpio59", "gpio60", "gpio61", "gpio62", "gpio63",
	"gpio64", "gpio65", "gpio66", "gpio67", "gpio68", "gpio69", "gpio70",
	"gpio71", "gpio72", "gpio73", "gpio74", "gpio75", "gpio76", "gpio77",
	"gpio78", "gpio79", "gpio80", "gpio81", "gpio82", "gpio83", "gpio84",
	"gpio85", "gpio86", "gpio87", "gpio88", "gpio89", "gpio90", "gpio91",
	"gpio92", "gpio93", "gpio94", "gpio95", "gpio96", "gpio97", "gpio98",
	"gpio99", "gpio100", "gpio101", "gpio102", "gpio103", "gpio104",
	"gpio105", "gpio106", "gpio107", "gpio108", "gpio109", "gpio110",
	"gpio111", "gpio112", "gpio113", "gpio114", "gpio115", "gpio116",
	"gpio117", "gpio118", "gpio119", "gpio120", "gpio121", "gpio122",
	"gpio123", "gpio124", "gpio125", "gpio126", "gpio127", "gpio128",
	"gpio129", "gpio130", "gpio131", "gpio132", "gpio133", "gpio134",
	"gpio135", "gpio136", "gpio137", "gpio138", "gpio139", "gpio140",
	"gpio141", "gpio142", "gpio143", "gpio144", "gpio145", "gpio146",
	"gpio147", "gpio148", "gpio149", "gpio150", "gpio151", "gpio152",
	"gpio153", "gpio154", "gpio155", "gpio156", "gpio157", "gpio158",
	"gpio159", "gpio160", "gpio161", "gpio162", "gpio163", "gpio164",
	"gpio165", "gpio166", "gpio167", "gpio168", "gpio169", "gpio170",
	"gpio171", "gpio172", "gpio173", "gpio174", "gpio175", "gpio176",
	"gpio177", "gpio178", "gpio179", "gpio180", "gpio181", "gpio182",
	"gpio183", "gpio184", "gpio185", "gpio186", "gpio187", "gpio188",
	"gpio189", "gpio190", "gpio191", "gpio192", "gpio193", "gpio194",
	"gpio195", "gpio196", "gpio197", "gpio198", "gpio199", "gpio200",
	"gpio201", "gpio202", "gpio203",
};
static const char * const atest_char_groups[] = {
	"gpio96",
};
static const char * const atest_char0_groups[] = {
	"gpio102",
};
static const char * const atest_char1_groups[] = {
	"gpio101",
};
static const char * const atest_char2_groups[] = {
	"gpio100",
};
static const char * const atest_char3_groups[] = {
	"gpio95",
};
static const char * const atest_usb0_groups[] = {
	"gpio202",
};
static const char * const atest_usb00_groups[] = {
	"gpio201",
};
static const char * const atest_usb01_groups[] = {
	"gpio4",
};
static const char * const atest_usb02_groups[] = {
	"gpio3",
};
static const char * const atest_usb03_groups[] = {
	"gpio194",
};
static const char * const audio_ref_groups[] = {
	"gpio124",
};
static const char * const cam_mclk_groups[] = {
	"gpio100", "gpio101", "gpio102", "gpio103", "gpio104", "gpio105",
};
static const char * const cci_async_groups[] = {
	"gpio106", "gpio118", "gpio119",
};
static const char * const cci_i2c_groups[] = {
	"gpio107", "gpio108", "gpio109", "gpio110", "gpio111", "gpio112",
	"gpio113", "gpio114",
};
static const char * const cci_timer0_groups[] = {
	"gpio115",
};
static const char * const cci_timer1_groups[] = {
	"gpio116",
};
static const char * const cci_timer2_groups[] = {
	"gpio117",
};
static const char * const cci_timer3_groups[] = {
	"gpio118",
};
static const char * const cci_timer4_groups[] = {
	"gpio119",
};
static const char * const cmu_rng0_groups[] = {
	"gpio177",
};
static const char * const cmu_rng1_groups[] = {
	"gpio176",
};
static const char * const cmu_rng2_groups[] = {
	"gpio175",
};
static const char * const cmu_rng3_groups[] = {
	"gpio174",
};
static const char * const coex_uart1_groups[] = {
	"gpio151", "gpio152",
};
static const char * const cri_trng_groups[] = {
	"gpio186",
};
static const char * const cri_trng0_groups[] = {
	"gpio183",
};
static const char * const cri_trng1_groups[] = {
	"gpio184",
};
static const char * const dbg_out_groups[] = {
	"gpio14",
};
static const char * const ddr_bist_groups[] = {
	"gpio36", "gpio37", "gpio40", "gpio41",
};
static const char * const ddr_pxi0_groups[] = {
	"gpio7", "gpio188",
};
static const char * const ddr_pxi1_groups[] = {
	"gpio5", "gpio6",
};
static const char * const dp_hot_groups[] = {
	"gpio87",
};
static const char * const dp_lcd_groups[] = {
	"gpio83",
};
static const char * const gcc_gp1_groups[] = {
	"gpio115", "gpio129",
};
static const char * const gcc_gp2_groups[] = {
	"gpio116", "gpio130",
};
static const char * const gcc_gp3_groups[] = {
	"gpio117", "gpio131",
};
static const char * const host2wlan_sol_groups[] = {
	"gpio201",
};
static const char * const ibi_i3c_groups[] = {
	"gpio36", "gpio37", "gpio56", "gpio57", "gpio60", "gpio61",
};
static const char * const jitter_bist_groups[] = {
	"gpio80",
};
static const char * const lpass_slimbus_groups[] = {
	"gpio129", "gpio130",
};
static const char * const mdp_vsync_groups[] = {
	"gpio15", "gpio26", "gpio82", "gpio83", "gpio84",
};
static const char * const mdp_vsync0_groups[] = {
	"gpio86",
};
static const char * const mdp_vsync1_groups[] = {
	"gpio86",
};
static const char * const mdp_vsync2_groups[] = {
	"gpio87",
};
static const char * const mdp_vsync3_groups[] = {
	"gpio87",
};
static const char * const mi2s0_data0_groups[] = {
	"gpio126",
};
static const char * const mi2s0_data1_groups[] = {
	"gpio127",
};
static const char * const mi2s0_sck_groups[] = {
	"gpio125",
};
static const char * const mi2s0_ws_groups[] = {
	"gpio128",
};
static const char * const mi2s1_data0_groups[] = {
	"gpio130",
};
static const char * const mi2s1_data1_groups[] = {
	"gpio131",
};
static const char * const mi2s1_sck_groups[] = {
	"gpio129",
};
static const char * const mi2s1_ws_groups[] = {
	"gpio132",
};
static const char * const mi2s2_data0_groups[] = {
	"gpio121",
};
static const char * const mi2s2_data1_groups[] = {
	"gpio124",
};
static const char * const mi2s2_sck_groups[] = {
	"gpio120",
};
static const char * const mi2s2_ws_groups[] = {
	"gpio122",
};
static const char * const mss_grfc0_groups[] = {
	"gpio141", "gpio158",
};
static const char * const mss_grfc1_groups[] = {
	"gpio142",
};
static const char * const mss_grfc10_groups[] = {
	"gpio151",
};
static const char * const mss_grfc11_groups[] = {
	"gpio152",
};
static const char * const mss_grfc12_groups[] = {
	"gpio157",
};
static const char * const mss_grfc2_groups[] = {
	"gpio143",
};
static const char * const mss_grfc3_groups[] = {
	"gpio144",
};
static const char * const mss_grfc4_groups[] = {
	"gpio145",
};
static const char * const mss_grfc5_groups[] = {
	"gpio146",
};
static const char * const mss_grfc6_groups[] = {
	"gpio147",
};
static const char * const mss_grfc7_groups[] = {
	"gpio148",
};
static const char * const mss_grfc8_groups[] = {
	"gpio149",
};
static const char * const mss_grfc9_groups[] = {
	"gpio150",
};
static const char * const nav_gpio_groups[] = {
	"gpio155", "gpio156", "gpio157",
};
static const char * const pa_indicator_groups[] = {
	"gpio157",
};
static const char * const pcie0_clkreqn_groups[] = {
	"gpio95",
};
static const char * const pcie1_clkreqn_groups[] = {
	"gpio98",
};
static const char * const phase_flag0_groups[] = {
	"gpio96",
};
static const char * const phase_flag1_groups[] = {
	"gpio95",
};
static const char * const phase_flag10_groups[] = {
	"gpio54",
};
static const char * const phase_flag11_groups[] = {
	"gpio53",
};
static const char * const phase_flag12_groups[] = {
	"gpio47",
};
static const char * const phase_flag13_groups[] = {
	"gpio45",
};
static const char * const phase_flag14_groups[] = {
	"gpio48",
};
static const char * const phase_flag15_groups[] = {
	"gpio44",
};
static const char * const phase_flag16_groups[] = {
	"gpio100",
};
static const char * const phase_flag17_groups[] = {
	"gpio49",
};
static const char * const phase_flag18_groups[] = {
	"gpio52",
};
static const char * const phase_flag19_groups[] = {
	"gpio42",
};
static const char * const phase_flag2_groups[] = {
	"gpio94",
};
static const char * const phase_flag20_groups[] = {
	"gpio39",
};
static const char * const phase_flag21_groups[] = {
	"gpio23",
};
static const char * const phase_flag22_groups[] = {
	"gpio22",
};
static const char * const phase_flag23_groups[] = {
	"gpio21",
};
static const char * const phase_flag24_groups[] = {
	"gpio20",
};
static const char * const phase_flag25_groups[] = {
	"gpio19",
};
static const char * const phase_flag26_groups[] = {
	"gpio18",
};
static const char * const phase_flag27_groups[] = {
	"gpio31",
};
static const char * const phase_flag28_groups[] = {
	"gpio38",
};
static const char * const phase_flag29_groups[] = {
	"gpio17",
};
static const char * const phase_flag3_groups[] = {
	"gpio200",
};
static const char * const phase_flag30_groups[] = {
	"gpio16",
};
static const char * const phase_flag31_groups[] = {
	"gpio46",
};
static const char * const phase_flag4_groups[] = {
	"gpio199",
};
static const char * const phase_flag5_groups[] = {
	"gpio82",
};
static const char * const phase_flag6_groups[] = {
	"gpio112",
};
static const char * const phase_flag7_groups[] = {
	"gpio87",
};
static const char * const phase_flag8_groups[] = {
	"gpio55",
};
static const char * const phase_flag9_groups[] = {
	"gpio43",
};
static const char * const pll_bist_groups[] = {
	"gpio81",
};
static const char * const pll_clk_groups[] = {
	"gpio81",
};
static const char * const pri_mi2s_groups[] = {
	"gpio123",
};
static const char * const prng_rosc_groups[] = {
	"gpio185",
};
static const char * const qdss_cti_groups[] = {
	"gpio14", "gpio27", "gpio87", "gpio88", "gpio89", "gpio90", "gpio91",
	"gpio92",
};
static const char * const qdss_gpio_groups[] = {
	"gpio109", "gpio110", "gpio191", "gpio192",
};
static const char * const qdss_gpio0_groups[] = {
	"gpio100", "gpio183",
};
static const char * const qdss_gpio1_groups[] = {
	"gpio101", "gpio184",
};
static const char * const qdss_gpio10_groups[] = {
	"gpio112", "gpio174",
};
static const char * const qdss_gpio11_groups[] = {
	"gpio113", "gpio175",
};
static const char * const qdss_gpio12_groups[] = {
	"gpio114", "gpio176",
};
static const char * const qdss_gpio13_groups[] = {
	"gpio115", "gpio177",
};
static const char * const qdss_gpio14_groups[] = {
	"gpio116", "gpio199",
};
static const char * const qdss_gpio15_groups[] = {
	"gpio117", "gpio200",
};
static const char * const qdss_gpio2_groups[] = {
	"gpio102", "gpio185",
};
static const char * const qdss_gpio3_groups[] = {
	"gpio103", "gpio186",
};
static const char * const qdss_gpio4_groups[] = {
	"gpio104", "gpio187",
};
static const char * const qdss_gpio5_groups[] = {
	"gpio105", "gpio188",
};
static const char * const qdss_gpio6_groups[] = {
	"gpio106", "gpio189",
};
static const char * const qdss_gpio7_groups[] = {
	"gpio107", "gpio190",
};
static const char * const qdss_gpio8_groups[] = {
	"gpio108", "gpio193",
};
static const char * const qdss_gpio9_groups[] = {
	"gpio111", "gpio194",
};
static const char * const qlink0_enable_groups[] = {
	"gpio160",
};
static const char * const qlink0_request_groups[] = {
	"gpio159",
};
static const char * const qlink0_wmss_groups[] = {
	"gpio161",
};
static const char * const qlink1_enable_groups[] = {
	"gpio163",
};
static const char * const qlink1_request_groups[] = {
	"gpio162",
};
static const char * const qlink1_wmss_groups[] = {
	"gpio164",
};
static const char * const qspi_clk_groups[] = {
	"gpio50",
};
static const char * const qspi_cs_groups[] = {
	"gpio47", "gpio80",
};
static const char * const qspi_data_groups[] = {
	"gpio44", "gpio45", "gpio48", "gpio49",
};
static const char * const qup0_groups[] = {
	"gpio40", "gpio41", "gpio42", "gpio43",
};
static const char * const qup1_groups[] = {
	"gpio36", "gpio37", "gpio38", "gpio39",
};
static const char * const qup10_groups[] = {
	"gpio20", "gpio21", "gpio22", "gpio23",
};
static const char * const qup11_groups[] = {
	"gpio8", "gpio9", "gpio10", "gpio11",
};
static const char * const qup12_groups[] = {
	"gpio24", "gpio25", "gpio26", "gpio27",
};
static const char * const qup13_groups[] = {
	"gpio16", "gpio17", "gpio18", "gpio19",
};
static const char * const qup14_groups[] = {
	"gpio32", "gpio33", "gpio34", "gpio35",
};
static const char * const qup15_groups[] = {
	"gpio68", "gpio69", "gpio70", "gpio71",
};
static const char * const qup2_groups[] = {
	"gpio44", "gpio45", "gpio46", "gpio47",
};
static const char * const qup3_groups[] = {
	"gpio48", "gpio49", "gpio50", "gpio80",
};
static const char * const qup4_groups[] = {
	"gpio52", "gpio53", "gpio54", "gpio55",
};
static const char * const qup5_groups[] = {
	"gpio0", "gpio1", "gpio2", "gpio3",
};
static const char * const qup6_groups[] = {
	"gpio14", "gpio15", "gpio30", "gpio31",
};
static const char * const qup7_groups[] = {
	"gpio4", "gpio5", "gpio6", "gpio7",
};
static const char * const qup8_groups[] = {
	"gpio56", "gpio57", "gpio58", "gpio59",
};
static const char * const qup9_groups[] = {
	"gpio60", "gpio61", "gpio62", "gpio63",
};
static const char * const qup_l4_groups[] = {
	"gpio2", "gpio6", "gpio58",
};
static const char * const qup_l5_groups[] = {
	"gpio3", "gpio7", "gpio59",
};
static const char * const qup_l6_groups[] = {
	"gpio10", "gpio42", "gpio62",
};
static const char * const sd_write_groups[] = {
	"gpio93",
};
static const char * const sdc40_groups[] = {
	"gpio44",
};
static const char * const sdc41_groups[] = {
	"gpio45",
};
static const char * const sdc42_groups[] = {
	"gpio48",
};
static const char * const sdc43_groups[] = {
	"gpio49",
};
static const char * const sdc4_clk_groups[] = {
	"gpio50",
};
static const char * const sdc4_cmd_groups[] = {
	"gpio80",
};
static const char * const sec_mi2s_groups[] = {
	"gpio124",
};
static const char * const tb_trig_groups[] = {
	"gpio43", "gpio64", "gpio136",
};
static const char * const tgu_ch0_groups[] = {
	"gpio99",
};
static const char * const tgu_ch1_groups[] = {
	"gpio100",
};
static const char * const tsense_pwm1_groups[] = {
	"gpio88",
};
static const char * const tsense_pwm2_groups[] = {
	"gpio88",
};
static const char * const uim0_clk_groups[] = {
	"gpio138",
};
static const char * const uim0_data_groups[] = {
	"gpio137",
};
static const char * const uim0_present_groups[] = {
	"gpio140",
};
static const char * const uim0_reset_groups[] = {
	"gpio139",
};
static const char * const uim1_clk_groups[] = {
	"gpio134",
};
static const char * const uim1_data_groups[] = {
	"gpio133",
};
static const char * const uim1_present_groups[] = {
	"gpio136",
};
static const char * const uim1_reset_groups[] = {
	"gpio135",
};
static const char * const usb2phy_ac_groups[] = {
	"gpio80",
};
static const char * const usb_phy_groups[] = {
	"gpio81",
};
static const char * const vfr_0_groups[] = {
	"gpio84",
};
static const char * const vfr_1_groups[] = {
	"gpio90",
};
static const char * const vsense_trigger_groups[] = {
	"gpio39",
};

static const struct msm_function shima_functions[] = {
	FUNCTION(gpio),
	FUNCTION(qup5),
	FUNCTION(qup_l4),
	FUNCTION(qup_l5),
	FUNCTION(atest_usb02),
	FUNCTION(qup7),
	FUNCTION(atest_usb01),
	FUNCTION(ddr_pxi1),
	FUNCTION(ddr_pxi0),
	FUNCTION(qup11),
	FUNCTION(qup_l6),
	FUNCTION(qup6),
	FUNCTION(qdss_cti),
	FUNCTION(dbg_out),
	FUNCTION(mdp_vsync),
	FUNCTION(qup13),
	FUNCTION(phase_flag30),
	FUNCTION(phase_flag29),
	FUNCTION(phase_flag26),
	FUNCTION(phase_flag25),
	FUNCTION(qup10),
	FUNCTION(phase_flag24),
	FUNCTION(phase_flag23),
	FUNCTION(phase_flag22),
	FUNCTION(phase_flag21),
	FUNCTION(qup12),
	FUNCTION(phase_flag27),
	FUNCTION(qup14),
	FUNCTION(qup1),
	FUNCTION(ibi_i3c),
	FUNCTION(ddr_bist),
	FUNCTION(phase_flag28),
	FUNCTION(phase_flag20),
	FUNCTION(vsense_trigger),
	FUNCTION(qup0),
	FUNCTION(phase_flag19),
	FUNCTION(phase_flag9),
	FUNCTION(tb_trig),
	FUNCTION(qup2),
	FUNCTION(qspi_data),
	FUNCTION(sdc40),
	FUNCTION(phase_flag15),
	FUNCTION(sdc41),
	FUNCTION(phase_flag13),
	FUNCTION(phase_flag31),
	FUNCTION(qspi_cs),
	FUNCTION(phase_flag12),
	FUNCTION(qup3),
	FUNCTION(sdc42),
	FUNCTION(phase_flag14),
	FUNCTION(sdc43),
	FUNCTION(phase_flag17),
	FUNCTION(qspi_clk),
	FUNCTION(sdc4_clk),
	FUNCTION(qup4),
	FUNCTION(phase_flag18),
	FUNCTION(phase_flag11),
	FUNCTION(phase_flag10),
	FUNCTION(phase_flag8),
	FUNCTION(qup8),
	FUNCTION(qup9),
	FUNCTION(qup15),
	FUNCTION(usb2phy_ac),
	FUNCTION(jitter_bist),
	FUNCTION(sdc4_cmd),
	FUNCTION(usb_phy),
	FUNCTION(pll_bist),
	FUNCTION(pll_clk),
	FUNCTION(phase_flag5),
	FUNCTION(dp_lcd),
	FUNCTION(vfr_0),
	FUNCTION(mdp_vsync0),
	FUNCTION(mdp_vsync1),
	FUNCTION(dp_hot),
	FUNCTION(mdp_vsync2),
	FUNCTION(mdp_vsync3),
	FUNCTION(phase_flag7),
	FUNCTION(tsense_pwm1),
	FUNCTION(tsense_pwm2),
	FUNCTION(vfr_1),
	FUNCTION(sd_write),
	FUNCTION(phase_flag2),
	FUNCTION(pcie0_clkreqn),
	FUNCTION(phase_flag1),
	FUNCTION(atest_char3),
	FUNCTION(phase_flag0),
	FUNCTION(atest_char),
	FUNCTION(pcie1_clkreqn),
	FUNCTION(tgu_ch0),
	FUNCTION(cam_mclk),
	FUNCTION(tgu_ch1),
	FUNCTION(qdss_gpio0),
	FUNCTION(phase_flag16),
	FUNCTION(atest_char2),
	FUNCTION(qdss_gpio1),
	FUNCTION(atest_char1),
	FUNCTION(qdss_gpio2),
	FUNCTION(atest_char0),
	FUNCTION(qdss_gpio3),
	FUNCTION(qdss_gpio4),
	FUNCTION(qdss_gpio5),
	FUNCTION(cci_async),
	FUNCTION(qdss_gpio6),
	FUNCTION(cci_i2c),
	FUNCTION(qdss_gpio7),
	FUNCTION(qdss_gpio8),
	FUNCTION(qdss_gpio),
	FUNCTION(qdss_gpio9),
	FUNCTION(phase_flag6),
	FUNCTION(qdss_gpio10),
	FUNCTION(qdss_gpio11),
	FUNCTION(qdss_gpio12),
	FUNCTION(cci_timer0),
	FUNCTION(gcc_gp1),
	FUNCTION(qdss_gpio13),
	FUNCTION(cci_timer1),
	FUNCTION(gcc_gp2),
	FUNCTION(qdss_gpio14),
	FUNCTION(cci_timer2),
	FUNCTION(gcc_gp3),
	FUNCTION(qdss_gpio15),
	FUNCTION(cci_timer3),
	FUNCTION(cci_timer4),
	FUNCTION(mi2s2_sck),
	FUNCTION(mi2s2_data0),
	FUNCTION(mi2s2_ws),
	FUNCTION(pri_mi2s),
	FUNCTION(sec_mi2s),
	FUNCTION(audio_ref),
	FUNCTION(mi2s2_data1),
	FUNCTION(mi2s0_sck),
	FUNCTION(mi2s0_data0),
	FUNCTION(mi2s0_data1),
	FUNCTION(mi2s0_ws),
	FUNCTION(lpass_slimbus),
	FUNCTION(mi2s1_sck),
	FUNCTION(mi2s1_data0),
	FUNCTION(mi2s1_data1),
	FUNCTION(mi2s1_ws),
	FUNCTION(uim1_data),
	FUNCTION(uim1_clk),
	FUNCTION(uim1_reset),
	FUNCTION(uim1_present),
	FUNCTION(uim0_data),
	FUNCTION(uim0_clk),
	FUNCTION(uim0_reset),
	FUNCTION(uim0_present),
	FUNCTION(mss_grfc0),
	FUNCTION(mss_grfc1),
	FUNCTION(mss_grfc2),
	FUNCTION(mss_grfc3),
	FUNCTION(mss_grfc4),
	FUNCTION(mss_grfc5),
	FUNCTION(mss_grfc6),
	FUNCTION(mss_grfc7),
	FUNCTION(mss_grfc8),
	FUNCTION(mss_grfc9),
	FUNCTION(coex_uart1),
	FUNCTION(mss_grfc10),
	FUNCTION(mss_grfc11),
	FUNCTION(nav_gpio),
	FUNCTION(mss_grfc12),
	FUNCTION(pa_indicator),
	FUNCTION(qlink0_request),
	FUNCTION(qlink0_enable),
	FUNCTION(qlink0_wmss),
	FUNCTION(qlink1_request),
	FUNCTION(qlink1_enable),
	FUNCTION(qlink1_wmss),
	FUNCTION(cmu_rng3),
	FUNCTION(cmu_rng2),
	FUNCTION(cmu_rng1),
	FUNCTION(cmu_rng0),
	FUNCTION(cri_trng0),
	FUNCTION(cri_trng1),
	FUNCTION(prng_rosc),
	FUNCTION(cri_trng),
	FUNCTION(atest_usb03),
	FUNCTION(phase_flag4),
	FUNCTION(phase_flag3),
	FUNCTION(host2wlan_sol),
	FUNCTION(atest_usb00),
	FUNCTION(atest_usb0),
};

/* Every pin is maintained as a single group, and missing or non-existing pin
 * would be maintained as dummy group to synchronize pin group index with
 * pin descriptor registered with pinctrl core.
 * Clients would not be able to request these dummy pin groups.
 */
static const struct msm_pingroup shima_groups[] = {
	[0] = PINGROUP(0, qup5, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[1] = PINGROUP(1, qup5, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[2] = PINGROUP(2, qup5, qup_l4, NA, NA, NA, NA, NA, NA, NA, 0xCC008, 4),
	[3] = PINGROUP(3, qup5, qup_l5, NA, NA, atest_usb02, NA, NA, NA, NA,
		       0xCC008, 5),
	[4] = PINGROUP(4, qup7, NA, NA, atest_usb01, NA, NA, NA, NA, NA, 0, -1),
	[5] = PINGROUP(5, qup7, NA, ddr_pxi1, NA, NA, NA, NA, NA, NA, 0, -1),
	[6] = PINGROUP(6, qup7, qup_l4, NA, ddr_pxi1, NA, NA, NA, NA, NA,
		       0, -1),
	[7] = PINGROUP(7, qup7, qup_l5, NA, ddr_pxi0, NA, NA, NA, NA, NA,
		       0xCC008, 6),
	[8] = PINGROUP(8, qup11, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[9] = PINGROUP(9, qup11, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[10] = PINGROUP(10, qup11, qup_l6, NA, NA, NA, NA, NA, NA, NA,
			0xCC000, 0),
	[11] = PINGROUP(11, qup11, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 1),
	[12] = PINGROUP(12, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[13] = PINGROUP(13, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[14] = PINGROUP(14, qup6, qdss_cti, dbg_out, NA, NA, NA, NA, NA, NA,
			0xCC008, 7),
	[15] = PINGROUP(15, qup6, mdp_vsync, NA, NA, NA, NA, NA, NA, NA,
			0xCC008, 8),
	[16] = PINGROUP(16, qup13, phase_flag30, NA, NA, NA, NA, NA, NA, NA,
			0xCC014, 0),
	[17] = PINGROUP(17, qup13, phase_flag29, NA, NA, NA, NA, NA, NA, NA,
			0xCC014, 1),
	[18] = PINGROUP(18, qup13, phase_flag26, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[19] = PINGROUP(19, qup13, phase_flag25, NA, NA, NA, NA, NA, NA, NA,
			0xCC014, 2),
	[20] = PINGROUP(20, qup10, NA, phase_flag24, NA, NA, NA, NA, NA, NA,
			0, -1),
	[21] = PINGROUP(21, qup10, NA, phase_flag23, NA, NA, NA, NA, NA, NA,
			0, -1),
	[22] = PINGROUP(22, qup10, NA, phase_flag22, NA, NA, NA, NA, NA, NA,
			0, -1),
	[23] = PINGROUP(23, qup10, phase_flag21, NA, NA, NA, NA, NA, NA, NA,
			0xCC014, 3),
	[24] = PINGROUP(24, qup12, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[25] = PINGROUP(25, qup12, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 2),
	[26] = PINGROUP(26, qup12, mdp_vsync, NA, NA, NA, NA, NA, NA, NA,
			0xCC000, 3),
	[27] = PINGROUP(27, qup12, qdss_cti, NA, NA, NA, NA, NA, NA, NA,
			0xCC000, 4),
	[28] = PINGROUP(28, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[29] = PINGROUP(29, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[30] = PINGROUP(30, qup6, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[31] = PINGROUP(31, qup6, phase_flag27, NA, NA, NA, NA, NA, NA, NA,
			0xCC008, 9),
	[32] = PINGROUP(32, qup14, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 5),
	[33] = PINGROUP(33, qup14, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[34] = PINGROUP(34, qup14, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 6),
	[35] = PINGROUP(35, qup14, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 7),
	[36] = PINGROUP(36, qup1, ibi_i3c, ddr_bist, NA, NA, NA, NA, NA, NA,
			0xCC008, 10),
	[37] = PINGROUP(37, qup1, ibi_i3c, ddr_bist, NA, NA, NA, NA, NA, NA,
			0, -1),
	[38] = PINGROUP(38, qup1, phase_flag28, NA, NA, NA, NA, NA, NA, NA,
			0xCC008, 11),
	[39] = PINGROUP(39, qup1, phase_flag20, NA, vsense_trigger, NA, NA, NA,
			NA, NA, 0xCC008, 12),
	[40] = PINGROUP(40, qup0, ddr_bist, NA, NA, NA, NA, NA, NA, NA,
			0xCC008, 13),
	[41] = PINGROUP(41, qup0, ddr_bist, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[42] = PINGROUP(42, qup0, qup_l6, phase_flag19, NA, NA, NA, NA, NA, NA,
			0, -1),
	[43] = PINGROUP(43, qup0, phase_flag9, tb_trig, NA, NA, NA, NA, NA, NA,
			0xCC008, 14),
	[44] = PINGROUP(44, qup2, qspi_data, sdc40, phase_flag15, NA, NA, NA,
			NA, NA, 0xCC008, 15),
	[45] = PINGROUP(45, qup2, qspi_data, sdc41, phase_flag13, NA, NA, NA,
			NA, NA, 0xCC00C, 0),
	[46] = PINGROUP(46, qup2, phase_flag31, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 1),
	[47] = PINGROUP(47, qup2, qspi_cs, phase_flag12, NA, NA, NA, NA, NA,
			NA, 0xCC00C, 2),
	[48] = PINGROUP(48, qup3, qspi_data, sdc42, phase_flag14, NA, NA, NA,
			NA, NA, 0xCC00C, 3),
	[49] = PINGROUP(49, qup3, qspi_data, sdc43, phase_flag17, NA, NA, NA,
			NA, NA, 0, -1),
	[50] = PINGROUP(50, qup3, qspi_clk, sdc4_clk, NA, NA, NA, NA, NA, NA,
			0xCC00C, 4),
	[51] = PINGROUP(51, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[52] = PINGROUP(52, qup4, phase_flag18, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[53] = PINGROUP(53, qup4, NA, phase_flag11, NA, NA, NA, NA, NA, NA,
			0, -1),
	[54] = PINGROUP(54, qup4, phase_flag10, NA, NA, NA, NA, NA, NA, NA,
			0, -1),
	[55] = PINGROUP(55, qup4, phase_flag8, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 5),
	[56] = PINGROUP(56, qup8, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
			0xCC000, 8),
	[57] = PINGROUP(57, qup8, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[58] = PINGROUP(58, qup8, qup_l4, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[59] = PINGROUP(59, qup8, qup_l5, NA, NA, NA, NA, NA, NA, NA,
			0xCC000, 9),
	[60] = PINGROUP(60, qup9, ibi_i3c, NA, NA, NA, NA, NA, NA, NA,
			0xCC000, 10),
	[61] = PINGROUP(61, qup9, ibi_i3c, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[62] = PINGROUP(62, qup9, qup_l6, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[63] = PINGROUP(63, qup9, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 11),
	[64] = PINGROUP(64, NA, tb_trig, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[65] = PINGROUP(65, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[66] = PINGROUP(66, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC00C, 6),
	[67] = PINGROUP(67, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[68] = PINGROUP(68, qup15, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[69] = PINGROUP(69, qup15, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[70] = PINGROUP(70, qup15, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[71] = PINGROUP(71, qup15, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 12),
	[72] = PINGROUP(72, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[73] = PINGROUP(73, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[74] = PINGROUP(74, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[75] = PINGROUP(75, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[76] = PINGROUP(76, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[77] = PINGROUP(77, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[78] = PINGROUP(78, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[79] = PINGROUP(79, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[80] = PINGROUP(80, usb2phy_ac, jitter_bist, qspi_cs, sdc4_cmd, qup3,
			NA, NA, NA, NA, 0xCC00C, 7),
	[81] = PINGROUP(81, usb_phy, pll_bist, pll_clk, NA, NA, NA, NA, NA, NA,
			0xCC00C, 8),
	[82] = PINGROUP(82, mdp_vsync, phase_flag5, NA, NA, NA, NA, NA, NA, NA,
			0xCC014, 4),
	[83] = PINGROUP(83, mdp_vsync, dp_lcd, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 9),
	[84] = PINGROUP(84, mdp_vsync, vfr_0, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 10),
	[85] = PINGROUP(85, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[86] = PINGROUP(86, mdp_vsync0, mdp_vsync1, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 11),
	[87] = PINGROUP(87, dp_hot, mdp_vsync2, mdp_vsync3, qdss_cti,
			phase_flag7, NA, NA, NA, NA, 0xCC014, 5),
	[88] = PINGROUP(88, qdss_cti, tsense_pwm1, tsense_pwm2, NA, NA, NA, NA,
			NA, NA, 0xCC00C, 12),
	[89] = PINGROUP(89, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 13),
	[90] = PINGROUP(90, vfr_1, qdss_cti, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 14),
	[91] = PINGROUP(91, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			0xCC00C, 15),
	[92] = PINGROUP(92, qdss_cti, NA, NA, NA, NA, NA, NA, NA, NA,
			0xCC010, 0),
	[93] = PINGROUP(93, sd_write, NA, NA, NA, NA, NA, NA, NA, NA,
			0xCC010, 1),
	[94] = PINGROUP(94, phase_flag2, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[95] = PINGROUP(95, pcie0_clkreqn, phase_flag1, NA, atest_char3, NA,
			NA, NA, NA, NA, 0xCC014, 6),
	[96] = PINGROUP(96, phase_flag0, NA, atest_char, NA, NA, NA, NA, NA,
			NA, 0xCC014, 7),
	[97] = PINGROUP(97, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[98] = PINGROUP(98, pcie1_clkreqn, NA, NA, NA, NA, NA, NA, NA, NA,
			0xCC010, 2),
	[99] = PINGROUP(99, tgu_ch0, NA, NA, NA, NA, NA, NA, NA, NA,
			0xCC010, 3),
	[100] = PINGROUP(100, cam_mclk, tgu_ch1, qdss_gpio0, phase_flag16, NA,
			 atest_char2, NA, NA, NA, 0, -1),
	[101] = PINGROUP(101, cam_mclk, NA, qdss_gpio1, atest_char1, NA, NA,
			 NA, NA, NA, 0, -1),
	[102] = PINGROUP(102, cam_mclk, NA, qdss_gpio2, atest_char0, NA, NA,
			 NA, NA, NA, 0, -1),
	[103] = PINGROUP(103, cam_mclk, NA, NA, qdss_gpio3, NA, NA, NA, NA, NA,
			 0, -1),
	[104] = PINGROUP(104, cam_mclk, NA, NA, qdss_gpio4, NA, NA, NA, NA, NA,
			 0xCC014, 8),
	[105] = PINGROUP(105, cam_mclk, NA, NA, qdss_gpio5, NA, NA, NA, NA, NA,
			 0xCC014, 9),
	[106] = PINGROUP(106, cci_async, NA, NA, qdss_gpio6, NA, NA, NA, NA,
			 NA, 0, -1),
	[107] = PINGROUP(107, cci_i2c, NA, NA, qdss_gpio7, NA, NA, NA, NA,
			 NA, 0xCC014, 10),
	[108] = PINGROUP(108, cci_i2c, NA, NA, qdss_gpio8, NA, NA, NA, NA,
			 NA, 0, -1),
	[109] = PINGROUP(109, cci_i2c, NA, NA, qdss_gpio, NA, NA, NA, NA, NA,
			 0, -1),
	[110] = PINGROUP(110, cci_i2c, NA, NA, qdss_gpio, NA, NA, NA, NA, NA,
			 0xCC014, 11),
	[111] = PINGROUP(111, cci_i2c, NA, NA, qdss_gpio9, NA, NA, NA, NA, NA,
			 0, -1),
	[112] = PINGROUP(112, cci_i2c, phase_flag6, NA, qdss_gpio10, NA, NA,
			 NA, NA, NA, 0, -1),
	[113] = PINGROUP(113, cci_i2c, NA, NA, qdss_gpio11, NA, NA, NA, NA, NA,
			 0xCC014, 12),
	[114] = PINGROUP(114, cci_i2c, NA, NA, qdss_gpio12, NA, NA, NA, NA, NA,
			 0xCC014, 13),
	[115] = PINGROUP(115, cci_timer0, gcc_gp1, qdss_gpio13, NA, NA, NA, NA,
			 NA, NA, 0xCC014, 14),
	[116] = PINGROUP(116, cci_timer1, gcc_gp2, qdss_gpio14, NA, NA, NA, NA,
			 NA, NA, 0xCC014, 15),
	[117] = PINGROUP(117, cci_timer2, gcc_gp3, qdss_gpio15, NA, NA, NA, NA,
			 NA, NA, 0xCC018, 0),
	[118] = PINGROUP(118, cci_timer3, cci_async, NA, NA, NA, NA, NA, NA,
			 NA, 0xCC010, 4),
	[119] = PINGROUP(119, cci_timer4, cci_async, NA, NA, NA, NA, NA, NA,
			 NA, 0xCC010, 5),
	[120] = PINGROUP(120, mi2s2_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[121] = PINGROUP(121, mi2s2_data0, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[122] = PINGROUP(122, mi2s2_ws, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[123] = PINGROUP(123, pri_mi2s, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[124] = PINGROUP(124, sec_mi2s, audio_ref, mi2s2_data1, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[125] = PINGROUP(125, mi2s0_sck, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[126] = PINGROUP(126, mi2s0_data0, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[127] = PINGROUP(127, mi2s0_data1, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[128] = PINGROUP(128, mi2s0_ws, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[129] = PINGROUP(129, lpass_slimbus, mi2s1_sck, gcc_gp1, NA, NA, NA,
			 NA, NA, NA, 0, -1),
	[130] = PINGROUP(130, lpass_slimbus, mi2s1_data0, gcc_gp2, NA, NA, NA,
			 NA, NA, NA, 0xCC000, 13),
	[131] = PINGROUP(131, mi2s1_data1, gcc_gp3, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[132] = PINGROUP(132, mi2s1_ws, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC000, 14),
	[133] = PINGROUP(133, uim1_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[134] = PINGROUP(134, uim1_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[135] = PINGROUP(135, uim1_reset, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[136] = PINGROUP(136, uim1_present, tb_trig, NA, NA, NA, NA, NA, NA,
			 NA, 0xCC010, 6),
	[137] = PINGROUP(137, uim0_data, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[138] = PINGROUP(138, uim0_clk, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[139] = PINGROUP(139, uim0_reset, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[140] = PINGROUP(140, uim0_present, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC010, 7),
	[141] = PINGROUP(141, NA, mss_grfc0, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[142] = PINGROUP(142, NA, mss_grfc1, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[143] = PINGROUP(143, NA, mss_grfc2, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[144] = PINGROUP(144, NA, mss_grfc3, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[145] = PINGROUP(145, NA, mss_grfc4, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[146] = PINGROUP(146, NA, mss_grfc5, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[147] = PINGROUP(147, NA, mss_grfc6, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[148] = PINGROUP(148, NA, mss_grfc7, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[149] = PINGROUP(149, NA, mss_grfc8, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[150] = PINGROUP(150, NA, mss_grfc9, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[151] = PINGROUP(151, coex_uart1, mss_grfc10, NA, NA, NA, NA, NA, NA,
			 NA, 0xCC010, 8),
	[152] = PINGROUP(152, coex_uart1, mss_grfc11, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[153] = PINGROUP(153, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[154] = PINGROUP(154, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[155] = PINGROUP(155, nav_gpio, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC010, 9),
	[156] = PINGROUP(156, nav_gpio, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC010, 10),
	[157] = PINGROUP(157, mss_grfc12, pa_indicator, nav_gpio, NA, NA, NA,
			 NA, NA, NA, 0xCC018, 1),
	[158] = PINGROUP(158, mss_grfc0, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[159] = PINGROUP(159, qlink0_request, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC018, 2),
	[160] = PINGROUP(160, qlink0_enable, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[161] = PINGROUP(161, qlink0_wmss, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[162] = PINGROUP(162, qlink1_request, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC018, 3),
	[163] = PINGROUP(163, qlink1_enable, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[164] = PINGROUP(164, qlink1_wmss, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[165] = PINGROUP(165, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[166] = PINGROUP(166, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[167] = PINGROUP(167, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[168] = PINGROUP(168, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[169] = PINGROUP(169, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC000, 15),
	[170] = PINGROUP(170, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[171] = PINGROUP(171, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[172] = PINGROUP(172, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC004, 0),
	[173] = PINGROUP(173, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[174] = PINGROUP(174, cmu_rng3, NA, qdss_gpio10, NA, NA, NA, NA, NA,
			 NA, 0xCC004, 1),
	[175] = PINGROUP(175, cmu_rng2, NA, qdss_gpio11, NA, NA, NA, NA, NA,
			 NA, 0xCC004, 2),
	[176] = PINGROUP(176, cmu_rng1, NA, qdss_gpio12, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[177] = PINGROUP(177, cmu_rng0, NA, qdss_gpio13, NA, NA, NA, NA, NA,
			 NA, 0xCC004, 3),
	[178] = PINGROUP(178, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[179] = PINGROUP(179, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC004, 4),
	[180] = PINGROUP(180, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0xCC004, 5),
	[181] = PINGROUP(181, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[182] = PINGROUP(182, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[183] = PINGROUP(183, cri_trng0, qdss_gpio0, NA, NA, NA, NA, NA, NA,
			 NA, 0xCC004, 6),
	[184] = PINGROUP(184, cri_trng1, qdss_gpio1, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[185] = PINGROUP(185, prng_rosc, qdss_gpio2, NA, NA, NA, NA, NA, NA,
			 NA, 0xCC004, 7),
	[186] = PINGROUP(186, cri_trng, qdss_gpio3, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[187] = PINGROUP(187, qdss_gpio4, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC004, 8),
	[188] = PINGROUP(188, qdss_gpio5, ddr_pxi0, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[189] = PINGROUP(189, qdss_gpio6, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[190] = PINGROUP(190, qdss_gpio7, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC004, 9),
	[191] = PINGROUP(191, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[192] = PINGROUP(192, qdss_gpio, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC004, 10),
	[193] = PINGROUP(193, qdss_gpio8, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[194] = PINGROUP(194, qdss_gpio9, atest_usb03, NA, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[195] = PINGROUP(195, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[196] = PINGROUP(196, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[197] = PINGROUP(197, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[198] = PINGROUP(198, NA, NA, NA, NA, NA, NA, NA, NA, NA, 0, -1),
	[199] = PINGROUP(199, phase_flag4, NA, qdss_gpio14, NA, NA, NA, NA, NA,
			 NA, 0, -1),
	[200] = PINGROUP(200, phase_flag3, NA, qdss_gpio15, NA, NA, NA, NA, NA,
			 NA, 0xCC004, 11),
	[201] = PINGROUP(201, host2wlan_sol, atest_usb00, NA, NA, NA, NA, NA,
			 NA, NA, 0, -1),
	[202] = PINGROUP(202, atest_usb0, NA, NA, NA, NA, NA, NA, NA, NA,
			 0xCC004, 12),
	[203] = PINGROUP(203, NA, NA, NA, NA, NA, NA, NA, NA, NA,
			 0, -1),
	[204] = UFS_RESET(ufs_reset, 0x1db000),
	[205] = SDC_QDSD_PINGROUP(sdc1_rclk, 0x1d0000, 15, 0),
	[206] = SDC_QDSD_PINGROUP(sdc1_clk, 0x1d0000, 13, 6),
	[207] = SDC_QDSD_PINGROUP(sdc1_cmd, 0x1d0000, 11, 3),
	[208] = SDC_QDSD_PINGROUP(sdc1_data, 0x1d0000, 9, 0),
	[209] = SDC_QDSD_PINGROUP(sdc2_clk, 0x1d1000, 14, 6),
	[210] = SDC_QDSD_PINGROUP(sdc2_cmd, 0x1d1000, 11, 3),
	[211] = SDC_QDSD_PINGROUP(sdc2_data, 0x1d1000, 9, 0),
};

static const int shima_reserved_gpios[] = {
	4, 5, 6, 7, 40, 41, 52, 53, 54, 55, 56, 57, 58, 59, -1
};

static struct pinctrl_qup shima_qup_regs[] = {
	QUP_I3C(0, QUP_I3C_0_MODE_OFFSET),
	QUP_I3C(1, QUP_I3C_1_MODE_OFFSET),
	QUP_I3C(8, QUP_I3C_8_MODE_OFFSET),
	QUP_I3C(9, QUP_I3C_9_MODE_OFFSET),
};

static const struct msm_gpio_wakeirq_map shima_pdc_map[] = {
	{ 2, 103 }, { 3, 104 }, { 7, 82 }, { 10, 163 }, { 11, 83 }, { 14, 80 },
	{ 15, 146 }, { 16, 155 }, { 17, 154 }, { 19, 121 }, { 23, 84 },
	{ 25, 135}, { 26, 86 }, { 27, 75 }, { 31, 85 }, { 32, 95 }, { 34, 98 },
	{ 35, 131}, { 36, 79 }, { 38, 99 }, { 39, 92 }, { 40, 101 },
	{ 43, 137 }, { 44, 102 }, { 45, 133 }, { 46, 96 }, { 47, 93 },
	{ 48, 127 }, { 50, 108 }, { 55, 128}, { 56, 81 }, { 59, 112 },
	{ 60, 119 }, { 63, 73 }, { 66, 74 }, { 71, 134 }, { 80, 126 },
	{ 81, 139 }, { 82, 140 }, { 83, 141 }, { 84, 124 }, { 86, 143 },
	{ 87, 138 }, { 88, 122 }, { 89, 113 }, { 90, 114 }, { 91, 115 },
	{ 92, 76 }, { 93, 117 }, { 95, 147 }, { 96, 148 }, { 98, 149 },
	{ 99, 150 }, { 104, 162}, { 105, 161 }, { 107, 160 }, { 110, 159 },
	{ 113, 158 }, { 114, 157 }, { 115, 125 }, { 116, 106 }, { 117, 105 },
	{ 118, 116 }, { 119, 123 }, { 130, 145 }, { 132, 156 }, { 136, 72 },
	{ 140, 100 }, { 151, 110 }, { 155, 107 }, { 156, 94 }, { 157, 111 },
	{ 159, 118 }, { 162, 77 }, { 169, 70 }, { 172, 132 }, { 174, 87 },
	{ 175, 88 }, { 177, 89 }, { 179, 120 }, { 180, 129 }, { 183, 90 },
	{ 185, 136 }, { 187, 142 }, { 190, 144 }, { 192, 164 }, { 200, 166 },
	{ 202, 165 },
};

static const struct msm_pinctrl_soc_data shima_pinctrl = {
	.pins = shima_pins,
	.npins = ARRAY_SIZE(shima_pins),
	.functions = shima_functions,
	.nfunctions = ARRAY_SIZE(shima_functions),
	.groups = shima_groups,
	.ngroups = ARRAY_SIZE(shima_groups),
	.reserved_gpios = shima_reserved_gpios,
	.ngpios = 205,
	.qup_regs = shima_qup_regs,
	.nqup_regs = ARRAY_SIZE(shima_qup_regs),
	.wakeirq_map = shima_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(shima_pdc_map),
};

static int shima_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &shima_pinctrl);
}

static const struct of_device_id shima_pinctrl_of_match[] = {
	{ .compatible = "qcom,shima-pinctrl", },
	{ },
};

static struct platform_driver shima_pinctrl_driver = {
	.driver = {
		.name = "shima-pinctrl",
		.of_match_table = shima_pinctrl_of_match,
	},
	.probe = shima_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init shima_pinctrl_init(void)
{
	return platform_driver_register(&shima_pinctrl_driver);
}
arch_initcall(shima_pinctrl_init);

static void __exit shima_pinctrl_exit(void)
{
	platform_driver_unregister(&shima_pinctrl_driver);
}
module_exit(shima_pinctrl_exit);

MODULE_DESCRIPTION("QTI shima pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, shima_pinctrl_of_match);
