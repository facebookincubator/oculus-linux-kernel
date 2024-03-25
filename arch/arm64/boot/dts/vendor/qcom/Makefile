ifneq ($(CONFIG_OCULUS),y)
ifneq ($(CONFIG_ARCH_QTI_VM), y)
ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LAHAINA) += lahaina-rumi-overlay.dtbo \
	lahaina-mtp-overlay.dtbo \
	lahaina-mtp-hsp-overlay.dtbo \
	lahaina-cdp-overlay.dtbo \
	lahaina-qrd-overlay.dtbo \
	lahaina-atp-overlay.dtbo \
	lahaina-hdk-overlay.dtbo \
	lahaina-qrd-hsp-overlay.dtbo \
	lahaina-qrd-module-overlay.dtbo \
	lahainap-mtp-overlay.dtbo \
	lahainap-cdp-overlay.dtbo \
	lahainap-atp-overlay.dtbo \
	lahainap-qrd-overlay.dtbo \
	lahaina-cdp-v2.2-overlay.dtbo \
	lahaina-mtp-v2-overlay.dtbo \
	lahaina-mtp-v2.1-overlay.dtbo

lahaina-rumi-overlay.dtbo-base := lahaina.dtb
lahaina-mtp-hsp-overlay.dtbo-base := lahaina.dtb
lahaina-mtp-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb lahaina-v2.1.dtb
lahaina-cdp-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb lahaina-v2.1.dtb
lahaina-atp-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb lahaina-v2.1.dtb
lahaina-hdk-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb lahaina-v2.1.dtb
lahaina-qrd-hsp-overlay.dtbo-base := lahaina.dtb
lahaina-qrd-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb lahaina-v2.1.dtb
lahaina-qrd-module-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb lahaina-v2.1.dtb
lahainap-mtp-overlay.dtbo-base := lahainap.dtb lahainap-v2.dtb lahainap-v2.1.dtb
lahainap-cdp-overlay.dtbo-base := lahainap.dtb lahainap-v2.dtb lahainap-v2.1.dtb
lahainap-atp-overlay.dtbo-base := lahainap.dtb lahainap-v2.dtb lahainap-v2.1.dtb
lahainap-qrd-overlay.dtbo-base := lahainap.dtb lahainap-v2.dtb lahainap-v2.1.dtb
lahaina-cdp-v2.2-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb \
					lahaina-v2.1.dtb
lahaina-mtp-v2-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb \
					lahaina-v2.1.dtb
lahaina-mtp-v2.1-overlay.dtbo-base := lahaina.dtb lahaina-v2.dtb \
					lahaina-v2.1.dtb
else
dtb-$(CONFIG_ARCH_LAHAINA) += lahaina-rumi.dtb \
	lahaina-mtp-hsp.dtb \
	lahaina-mtp.dtb \
	lahaina-cdp.dtb \
	lahaina-atp.dtb \
	lahaina-hdk.dtb \
	lahaina-qrd-hsp.dtb \
	lahaina-qrd.dtb \
	lahaina-qrd-module.dtb \
	lahainap-mtp.dtb \
	lahainap-cdp.dtb \
	lahainap-atp.dtb \
	lahainap-qrd.dtb \
	lahaina-v2-mtp.dtb \
	lahaina-v2-cdp.dtb \
	lahaina-v2-atp.dtb \
	lahaina-v2-hdk.dtb \
	lahaina-v2-qrd.dtb \
	lahaina-v2-qrd-module.dtb \
	lahainap-v2-mtp.dtb \
	lahainap-v2-cdp.dtb \
	lahainap-v2-atp.dtb \
	lahainap-v2-qrd.dtb \
	lahaina-cdp-v2.2.dtb \
	lahaina-v2.1-mtp.dtb \
	lahaina-v2.1-cdp.dtb \
	lahaina-v2.1-atp.dtb \
	lahaina-v2.1-hdk.dtb \
	lahaina-v2.1-qrd.dtb \
	lahaina-v2.1-qrd-module.dtb \
	lahainap-v2.1-mtp.dtb \
	lahainap-v2.1-cdp.dtb \
	lahainap-v2.1-atp.dtb \
	lahainap-v2.1-qrd.dtb \
	lahaina-mtp-v2.dtb \
	lahaina-mtp-v2.1.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_WAIPIO) += waipio-rumi-overlay.dtbo \
	waipio-mtp-pm8008-overlay.dtbo \
	waipio-cdp-pm8008-overlay.dtbo \
	waipio-qrd-pm8008-overlay.dtbo \
	waipio-atp-pm8008-overlay.dtbo \
	waipio-mtp-pm8010-overlay.dtbo \
	waipio-cdp-pm8010-overlay.dtbo \
	waipio-qrd-pm8010-overlay.dtbo \
	waipio-qrd-pm8010-2s-overlay.dtbo \
	waipio-atp-pm8010-overlay.dtbo \
	waipiop-hdk-pm8010-overlay.dtbo \
	waipio-lemur-mtp-pm8008-overlay.dtbo \
	waipio-lemur-mtp-pm8010-overlay.dtbo \
	waipio-lemur-cdp-pm8008-overlay.dtbo \
	waipio-lemur-cdp-pm8010-overlay.dtbo \
	waipio-kiwi-mtp-pm8008-overlay.dtbo \
	waipio-kiwi-mtp-pm8010-overlay.dtbo \
	waipio-kiwi-cdp-pm8008-overlay.dtbo \
	waipio-kiwi-cdp-pm8010-overlay.dtbo \
	waipio-mtp-68disp-pm8008-overlay.dtbo \
	waipio-mtp-68disp-pm8010-overlay.dtbo \
	waipio-cdp-xr-overlay.dtbo \
	waipio-mtp-xr-overlay.dtbo \
	waipio-cdp-xr-pcie-overlay.dtbo \
	waipio-mtp-xr-pcie-overlay.dtbo

waipio-rumi-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb
waipio-mtp-pm8008-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-mtp-68disp-pm8008-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-cdp-pm8008-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-qrd-pm8008-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-atp-pm8008-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-mtp-pm8010-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-mtp-68disp-pm8010-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-cdp-pm8010-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-qrd-pm8010-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-qrd-pm8010-2s-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb
waipio-atp-pm8010-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipiop-hdk-pm8010-overlay.dtbo-base := waipiop.dtb waipiop-v2.dtb
waipio-lemur-mtp-pm8008-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-lemur-mtp-pm8010-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-lemur-cdp-pm8008-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-lemur-cdp-pm8010-overlay.dtbo-base := waipio.dtb waipiop.dtb waipio-v2.dtb waipiop-v2.dtb waipio-lte.dtb
waipio-kiwi-mtp-pm8008-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-lte.dtb
waipio-kiwi-mtp-pm8010-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-lte.dtb
waipio-kiwi-cdp-pm8008-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-lte.dtb
waipio-kiwi-cdp-pm8010-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-lte.dtb
waipio-cdp-xr-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-le.dtb
waipio-mtp-xr-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-le.dtb
waipio-cdp-xr-pcie-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-le.dtb
waipio-mtp-xr-pcie-overlay.dtbo-base := waipio.dtb waipio-v2.dtb waipio-le.dtb
else
dtb-$(CONFIG_ARCH_WAIPIO) += waipio-rumi.dtb \
	waipio-mtp-pm8008.dtb \
	waipio-cdp-pm8008.dtb \
	waipio-qrd-pm8008.dtb \
	waipio-atp-pm8008.dtb \
	waipio-mtp-pm8010.dtb \
	waipio-cdp-pm8010.dtb \
	waipio-qrd-pm8010.dtb \
	waipio-qrd-pm8010-2s.dtb \
	waipio-atp-pm8010.dtb \
	waipio-cdp-xr.dtb \
	waipio-mtp-xr.dtb \
	waipiop-mtp-pm8008.dtb \
	waipiop-cdp-pm8008.dtb \
	waipiop-qrd-pm8008.dtb \
	waipiop-mtp-pm8010.dtb \
	waipiop-cdp-pm8010.dtb \
	waipiop-qrd-pm8010.dtb \
	waipiop-hdk-pm8010.dtb \
	waipiop-qrd-pm8010-2s.dtb
# Waipio V2
dtb-$(CONFIG_ARCH_WAIPIO) += waipio-v2-mtp-pm8008.dtb \
	waipio-v2-cdp-pm8008.dtb \
	waipio-v2-qrd-pm8008.dtb \
	waipio-v2-atp-pm8008.dtb \
	waipio-v2-mtp-pm8010.dtb \
	waipio-v2-cdp-pm8010.dtb \
	waipio-v2-qrd-pm8010.dtb \
	waipio-v2-qrd-pm8010-2s.dtb \
	waipio-v2-atp-pm8010.dtb \
	waipiop-v2-mtp-pm8008.dtb \
	waipiop-v2-cdp-pm8008.dtb \
	waipiop-v2-qrd-pm8008.dtb \
	waipiop-v2-mtp-pm8010.dtb \
	waipiop-v2-cdp-pm8010.dtb \
	waipiop-v2-qrd-pm8010.dtb \
	waipiop-v2-hdk-pm8010.dtb \
	waipiop-v2-qrd-pm8010-2s.dtb

# Waipio LTE
dtb-$(CONFIG_ARCH_WAIPIO) += waipio-lte-mtp-pm8008.dtb \
	waipio-lte-cdp-pm8008.dtb \
	waipio-lte-qrd-pm8008.dtb \
	waipio-lte-atp-pm8008.dtb \
	waipio-lte-mtp-pm8010.dtb \
	waipio-lte-cdp-pm8010.dtb \
	waipio-lte-qrd-pm8010.dtb \
	waipio-lte-atp-pm8010.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_NEO) += neo-rumi-overlay.dtbo \
		neo_la-rumi-overlay.dtbo \
		neo-qxr-mos-overlay.dtbo \
		neo-qxr-hmt-overlay.dtbo \
		neo-qxr-e2e-overlay.dtbo \
		foreseer-qxr-hmt-overlay.dtbo \
		neo-idp-mos-overlay.dtbo \
		neo-atp-overlay.dtbo \
		neo-sg-idp-hmt-overlay.dtbo \
		neo-idp-display-overlay.dtbo \
		neo-idp-hmt-overlay.dtbo \
		neo-idp-e2e-overlay.dtbo \
		neo-sg-idp-pm8008-overlay.dtbo \
		neo_luna-v2-sg-idp-overlay.dtbo \
		neo_luna-v2-sg-idp-pm8008-overlay.dtbo \
		neo_luna-v2-sg-idp-dual-display-overlay.dtbo \
		neo_luna-v2-sg-idp-dual-display-camera-overlay.dtbo

neo-rumi-overlay.dtbo-base := neo.dtb
neo_la-rumi-overlay.dtbo-base := neo_la.dtb
neo-qxr-mos-overlay.dtbo-base := neo.dtb
neo-qxr-hmt-overlay.dtbo-base := neo.dtb
neo-qxr-e2e-overlay.dtbo-base := neo.dtb
foreseer-qxr-hmt-overlay.dtbo-base := neo.dtb
neo-idp-mos-overlay.dtbo-base := neo.dtb
neo-atp-overlay.dtbo-base := neo.dtb
neo-sg-idp-hmt-overlay.dtbo-base := neo_la.dtb
neo-idp-display-overlay.dtbo-base := neo.dtb
neo-idp-hmt-overlay.dtbo-base := neo.dtb
neo-idp-e2e-overlay.dtbo-base := neo.dtb
neo-sg-idp-pm8008-overlay.dtbo-base := neo_la.dtb
neo_luna-v2-sg-idp-overlay.dtbo-base := neo_luna-v2.dtb
neo_luna-v2-sg-idp-pm8008-overlay.dtbo-base := neo_luna-v2.dtb
neo_luna-v2-sg-idp-dual-display-overlay.dtbo-base := neo_luna-v2.dtb
neo_luna-v2-sg-idp-dual-display-camera-overlay.dtbo-base := neo_luna-v2.dtb

else
dtb-$(CONFIG_ARCH_NEO) += neo-rumi.dtb \
			neo_la-rumi.dtb \
			neo-qxr-mos.dtb \
			neo-qxr-hmt.dtb \
			neo-qxr-e2e.dtb \
			foreseer-qxr-hmt.dtb \
			neo-idp-mos.dtb \
			neo-atp.dtb \
			neo-sg-idp-hmt.dtb \
			neo-idp-display.dtb \
			neo-idp-hmt.dtb \
			neo-idp-e2e.dtb \
			neo-sg-idp-pm8008.dtb \
			neo_luna-v2-sg-idp.dtb \
			neo_luna-v2-sg-idp-pm8008.dtb \
			neo_luna-v2-sg-idp-dual-display.dtb \
			neo_luna-v2-sg-idp-dual-display-camera.dtb

endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_DIWALI) += diwali-rumi-overlay.dtbo \
	diwali-idp-overlay.dtbo \
	diwali-atp-overlay.dtbo \
	diwali-qrd-overlay.dtbo \
	diwali-idp-amoled-overlay.dtbo \
	diwali-idp-usbc-overlay.dtbo \
	diwali-idp-hsp-overlay.dtbo \
	diwali-idp-nopmi-overlay.dtbo \
	diwali-qrd-nopmi-overlay.dtbo \
	diwali-qrd-pm8350b-overlay.dtbo

diwali-rumi-overlay.dtbo-base := diwali.dtb
diwali-idp-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
diwali-atp-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
diwali-qrd-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
diwali-idp-amoled-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
diwali-idp-usbc-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
diwali-idp-hsp-overlay.dtbo-base := diwali-hsp.dtb diwalip.dtb diwali-lte.dtb
diwali-idp-nopmi-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
diwali-qrd-nopmi-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
diwali-qrd-pm8350b-overlay.dtbo-base := diwali.dtb diwalip.dtb diwali-lte.dtb
else
dtb-$(CONFIG_ARCH_DIWALI) += diwali-rumi.dtb \
	diwali-idp.dtb \
	diwali-atp.dtb \
	diwali-qrd.dtb \
	diwali-idp-amoled.dtb \
	diwali-idp-usbc.dtb \
	diwali-idp-hsp.dtb \
	diwali-idp-nopmi.dtb \
	diwali-qrd-nopmi.dtb \
	diwali-qrd-pm8350b.dtb \
	diwalip-idp.dtb \
	diwalip-atp.dtb \
	diwalip-qrd.dtb \
	diwalip-idp-amoled.dtb \
	diwalip-idp-usbc.dtb \
	diwalip-idp-hsp.dtb \
	diwalip-idp-nopmi.dtb \
	diwalip-qrd-nopmi.dtb \
	diwalip-qrd-pm8350b.dtb \
	diwali-lte-idp.dtb \
	diwali-lte-atp.dtb \
	diwali-lte-qrd.dtb \
	diwali-lte-idp-amoled.dtb \
	diwali-lte-idp-usbc.dtb \
	diwali-lte-idp-hsp.dtb \
	diwali-lte-idp-nopmi.dtb \
	diwali-lte-qrd-nopmi.dtbi \
	diwali-lte-qrd-pm8350b.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_CAPE) += cape-mtp-pm8008-overlay.dtbo \
        cape-mtp-120fps-pm8008-overlay.dtbo \
	cape-mtp-nodisplay-pm8008-overlay.dtbo \
        cape-cdp-pm8008-overlay.dtbo \
        cape-cdp-qhd-pm8008-overlay.dtbo \
        cape-atp-pm8008-overlay.dtbo \
        cape-qrd-pm8008-overlay.dtbo \
        cape-mtp-pm8010-overlay.dtbo \
        cape-mtp-120fps-pm8010-overlay.dtbo \
	cape-mtp-nodisplay-pm8010-overlay.dtbo \
        cape-cdp-pm8010-overlay.dtbo \
        cape-atp-pm8010-overlay.dtbo \
        cape-qrd-pm8010-overlay.dtbo \
        cape-cdp-qhd-pm8010-overlay.dtbo \
	cape-kiwi-mtp-pm8008-overlay.dtbo \
	cape-kiwi-mtp-pm8010-overlay.dtbo \
	cape-kiwi-cdp-pm8008-overlay.dtbo \
	cape-kiwi-cdp-pm8010-overlay.dtbo \
	ukee-atp-pm8008-overlay.dtbo \
	ukee-atp-pm8010-overlay.dtbo \
	ukee-cdp-pm8008-overlay.dtbo \
	ukee-cdp-pm8010-overlay.dtbo \
	ukee-cdp-qhd-pm8008-overlay.dtbo \
	ukee-cdp-qhd-pm8010-overlay.dtbo \
	ukee-mtp-pm8008-overlay.dtbo \
	ukee-mtp-pm8010-overlay.dtbo \
	ukee-mtp-120fps-pm8008-overlay.dtbo \
	ukee-mtp-120fps-pm8010-overlay.dtbo \
	ukee-mtp-nodisplay-pm8008-overlay.dtbo \
	ukee-mtp-nodisplay-pm8010-overlay.dtbo \
	ukee-qrd-pm8008-overlay.dtbo \
	ukee-qrd-pm8010-overlay.dtbo

cape-mtp-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-mtp-120fps-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-mtp-nodisplay-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-cdp-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-cdp-qhd-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-atp-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-qrd-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-mtp-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-mtp-120fps-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-mtp-nodisplay-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-cdp-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-cdp-qhd-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-atp-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-qrd-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-kiwi-mtp-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-kiwi-mtp-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-kiwi-cdp-pm8008-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
cape-kiwi-cdp-pm8010-overlay.dtbo-base := cape.dtb cape-v2.dtb capep.dtb
ukee-atp-pm8008-overlay.dtbo-base := ukee.dtb
ukee-atp-pm8010-overlay.dtbo-base := ukee.dtb
ukee-cdp-pm8008-overlay.dtbo-base := ukee.dtb
ukee-cdp-pm8010-overlay.dtbo-base := ukee.dtb
ukee-cdp-qhd-pm8008-overlay.dtbo-base := ukee.dtb
ukee-cdp-qhd-pm8010-overlay.dtbo-base := ukee.dtb
ukee-mtp-pm8008-overlay.dtbo-base := ukee.dtb
ukee-mtp-pm8010-overlay.dtbo-base := ukee.dtb
ukee-mtp-120fps-pm8008-overlay.dtbo-base := ukee.dtb
ukee-mtp-120fps-pm8010-overlay.dtbo-base := ukee.dtb
ukee-mtp-nodisplay-pm8008-overlay.dtbo-base := ukee.dtb
ukee-mtp-nodisplay-pm8010-overlay.dtbo-base := ukee.dtb
ukee-qrd-pm8008-overlay.dtbo-base := ukee.dtb
ukee-qrd-pm8010-overlay.dtbo-base := ukee.dtb

else
dtb-$(CONFIG_ARCH_CAPE) += cape-mtp-pm8008.dtb \
	cape-mtp-120fps-pm8008.dtb \
	cape-mtp-nodisplay-pm8008.dtb \
        cape-cdp-pm8008.dtb \
        cape-cdp-qhd-pm8008.dtb \
        cape-atp-pm8008.dtb \
        cape-qrd-pm8008.dtb \
        cape-v2-mtp-pm8008.dtb \
        cape-v2-cdp-pm8008.dtb \
        cape-v2-atp-pm8008.dtb \
        cape-v2-qrd-pm8008.dtb \
	capep-mtp-pm8008.dtb \
	capep-cdp-pm8008.dtb \
	capep-atp-pm8008.dtb \
        cape-mtp-pm8010.dtb \
        cape-mtp-120fps-pm8010.dtb \
	cape-mtp-nodisplay-pm8010.dtb \
        cape-cdp-pm8010.dtb \
        cape-cdp-qhd-pm8010.dtb \
        cape-atp-pm8010.dtb \
        cape-qrd-pm8010.dtb \
        cape-v2-mtp-pm8010.dtb \
        cape-v2-cdp-pm8010.dtb \
        cape-v2-atp-pm8010.dtb \
        cape-v2-qrd-pm8010.dtb \
	capep-mtp-pm8010.dtb \
	capep-cdp-pm8010.dtb \
	capep-atp-pm8010.dtb \
	ukee-atp-pm8008.dtb \
	ukee-atp-pm8010.dtb \
	ukee-cdp-pm8008.dtb \
	ukee-cdp-pm8010.dtb \
	ukee-cdp-qhd-pm8008.dtb \
	ukee-cdp-qhd-pm8010.dtb \
	ukee-mtp-pm8008.dtb \
	ukee-mtp-pm8010.dtb \
	ukee-mtp-120fps-pm8008.dtb \
	ukee-mtp-120fps-pm8010.dtb \
	ukee-mtp-nodisplay-pm8008.dtb \
	ukee-mtp-nodisplay-pm8010.dtb \
	ukee-qrd-pm8008.dtb \
	ukee-qrd-pm8010.dtb

endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_PARROT) += parrot-rumi-overlay.dtbo \
	parrot-atp-overlay.dtbo \
	parrot-idp-overlay.dtbo \
	parrot-idp-4gb-overlay.dtbo \
	parrot-idp-wcn3990-overlay.dtbo \
	parrot-idp-wcn3990-4gb-overlay.dtbo \
	parrot-idp-wcn3990-amoled-rcm-overlay.dtbo \
	parrot-idp-wcn3990-amoled-rcm-4gb-overlay.dtbo \
	parrot-idp-wcn6750-amoled-rcm-overlay.dtbo \
	parrot-idp-wcn6750-amoled-rcm-4gb-overlay.dtbo \
	parrot-idp-wcn6750-amoled-overlay.dtbo \
	parrot-idp-wcn6750-amoled-4gb-overlay.dtbo \
	parrot-idp-nopmi-overlay.dtbo \
	parrot-idp-pm8350b-overlay.dtbo \
	parrot-qrd-overlay.dtbo \
	parrot-qrd-4gb-overlay.dtbo \
	parrot-qrd-wcn6750-overlay.dtbo \
	parrot-qrd-wcn6750-4gb-overlay.dtbo \
	parrot-qrd-nopmi-overlay.dtbo	\
	parrot-qrd-pm8350b-overlay.dtbo

parrot-rumi-overlay.dtbo-base := parrot.dtb
parrot-atp-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-idp-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-idp-4gb-overlay.dtbo-base := parrot-4gb.dtb
parrot-idp-wcn3990-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-idp-wcn3990-4gb-overlay.dtbo-base := parrot-4gb.dtb
parrot-idp-wcn3990-amoled-rcm-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-idp-wcn3990-amoled-rcm-4gb-overlay.dtbo-base := parrot-4gb.dtb
parrot-idp-wcn6750-amoled-rcm-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-idp-wcn6750-amoled-rcm-4gb-overlay.dtbo-base := parrot-4gb.dtb
parrot-idp-wcn6750-amoled-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-idp-wcn6750-amoled-4gb-overlay.dtbo-base := parrot-4gb.dtb
parrot-idp-nopmi-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-idp-pm8350b-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-qrd-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-qrd-4gb-overlay.dtbo-base := parrot-4gb.dtb
parrot-qrd-wcn6750-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-qrd-wcn6750-4gb-overlay.dtbo-base := parrot-4gb.dtb
parrot-qrd-nopmi-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
parrot-qrd-pm8350b-overlay.dtbo-base := parrot.dtb parrotp.dtb parrot-sg.dtb parrotp-sg.dtb
else
dtb-$(CONFIG_ARCH_PARROT) += parrot-rumi.dtb \
	parrot-atp.dtb \
	parrot-idp.dtb \
	parrot-idp-4gb.dtb \
	parrot-idp-wcn3990.dtb \
	parrot-idp-wcn3990-4gb.dtb \
	parrot-idp-wcn3990-amoled-rcm.dtb \
	parrot-idp-wcn3990-amoled-rcm-4gb.dtb \
	parrot-idp-wcn6750-amoled.dtb \
	parrot-idp-wcn6750-amoled-4gb.dtb \
	parrot-idp-nopmi.dtb \
	parrot-idp-pm8350b.dtb \
	parrot-idp-wcn6750-amoled-rcm.dtb \
	parrot-idp-wcn6750-amoled-rcm-4gb.dtb \
	parrot-qrd.dtb \
	parrot-qrd-4gb.dtb \
	parrot-qrd-wcn6750.dtb \
	parrot-qrd-wcn6750-4gb.dtb \
	parrot-qrd-nopmi.dtb \
	parrot-qrd-pm8350b.dtb \
	parrotp-atp.dtb \
	parrotp-idp.dtb \
	parrotp-idp-wcn3990.dtb \
	parrotp-idp-wcn3990-amoled-rcm.dtb \
	parrotp-idp-wcn6750-amoled.dtb \
	parrotp-idp-wcn6750-amoled-rcm.dtb \
	parrotp-idp-nopmi.dtb \
	parrotp-idp-pm8350b.dtb \
	parrotp-qrd.dtb \
	parrotp-qrd-wcn6750.dtb \
	parrotp-qrd-nopmi.dtb	\
	parrotp-qrd-pm8350b.dtb \
	parrot-sg-atp.dtb \
	parrot-sg-idp.dtb \
	parrot-sg-idp-wcn3990.dtb \
	parrot-sg-idp-wcn3990-amoled-rcm.dtb \
	parrot-sg-idp-wcn6750-amoled.dtb \
	parrot-sg-idp-wcn6750-amoled-rcm.dtb \
	parrot-sg-idp-nopmi.dtb \
	parrot-sg-idp-pm8350b.dtb \
	parrot-sg-qrd.dtb \
	parrot-sg-qrd-wcn6750.dtb \
	parrot-sg-qrd-nopmi.dtb \
	parrot-sg-qrd-pm8350b.dtb \
	parrotp-sg-atp.dtb \
	parrotp-sg-idp.dtb \
	parrotp-sg-idp-wcn3990.dtb \
	parrotp-sg-idp-wcn3990-amoled-rcm.dtb \
	parrotp-sg-idp-wcn6750-amoled.dtb \
	parrotp-sg-idp-wcn6750-amoled-rcm.dtb \
	parrotp-sg-idp-nopmi.dtb \
	parrotp-sg-idp-pm8350b.dtb \
	parrotp-sg-qrd.dtb \
	parrotp-sg-qrd-wcn6750.dtb \
	parrotp-sg-qrd-nopmi.dtb \
	parrotp-sg-qrd-pm8350b.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_HOLI) +=  holi-rumi-overlay.dtbo
dtbo-$(CONFIG_ARCH_HOLI) += holi-rumi-overlay.dtbo \
	holi-mtp-overlay.dtbo \
	holi-cdp-overlay.dtbo \
	holi-cdp-lcd-overlay.dtbo \
	holi-qrd-overlay.dtbo \
	holi-atp-overlay.dtbo \
	holi-mtp-usbc-overlay.dtbo

holi-rumi-overlay.dtbo-base := holi.dtb
holi-mtp-overlay.dtbo-base := holi.dtb
holi-cdp-overlay.dtbo-base := holi.dtb
holi-cdp-lcd-overlay.dtbo-base := holi.dtb
holi-qrd-overlay.dtbo-base := holi.dtb
holi-atp-overlay.dtbo-base := holi.dtb
holi-mtp-usbc-overlay.dtbo-base := holi.dtb
else
dtb-$(CONFIG_ARCH_HOLI) += holi-rumi.dtb \
	holi-mtp.dtb \
	holi-cdp.dtb \
	holi-cdp-lcd.dtb \
	holi-qrd.dtb \
	holi-atp.dtb \
	holi-mtp-usbc.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_SHIMA) +=    \
	shima-rumi-overlay.dtbo \
	shima-idp-overlay.dtbo	\
	shima-atp-overlay.dtbo \
	shima-qrd-overlay.dtbo \
	shima-idps-digital-mics-overlay.dtbo

shima-rumi-overlay.dtbo-base := shima.dtb
shima-idp-overlay.dtbo-base := shima.dtb
shima-atp-overlay.dtbo-base := shima.dtb
shima-qrd-overlay.dtbo-base := shima.dtb
shima-idps-digital-mics-overlay.dtbo-base := shima.dtb
else
dtb-$(CONFIG_ARCH_SHIMA) += shima-rumi.dtb  \
			    shima-idp.dtb   \
			    shima-atp.dtb \
			    shima-qrd.dtb \
			    shima-idps-digital-mics.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_RAVELIN) += ravelin-rumi-overlay.dtbo \
	ravelin-atp-overlay.dtbo \
	ravelin-idp-overlay.dtbo \
	ravelin-idp-wcn3988-4gb-overlay.dtbo \
	ravelin-idp-wcn3950-amoled-rcm-overlay.dtbo \
	ravelin-qrd-overlay.dtbo \
	ravelin-qrd-4gb-overlay.dtbo \
	montague-rumi-overlay.dtbo \
	montague-hsp-overlay.dtbo \
	montague-talyn-overlay.dtbo \
	montague-moselle-overlay.dtbo \
	montague-rcm-overlay.dtbo \
	montague-wsa-overlay.dtbo \
	montague-wsd-overlay.dtbo

ravelin-rumi-overlay.dtbo-base := ravelin.dtb montague.dtb
ravelin-atp-overlay.dtbo-base := ravelin.dtb ravelinp.dtb montague.dtb montaguep.dtb
ravelin-idp-overlay.dtbo-base := ravelin.dtb ravelinp.dtb montague.dtb montaguep.dtb
ravelin-idp-wcn3988-4gb-overlay.dtbo-base := ravelin-4gb.dtb ravelinp-4gb.dtb
ravelin-idp-wcn3950-amoled-rcm-overlay.dtbo-base := ravelin.dtb ravelinp.dtb montague.dtb montaguep.dtb
ravelin-qrd-overlay.dtbo-base := ravelin.dtb ravelinp.dtb montague.dtb montaguep.dtb
ravelin-qrd-4gb-overlay.dtbo-base := ravelin-4gb.dtb ravelinp-4gb.dtb montague.dtb montaguep.dtb
montague-rumi-overlay.dtbo-base := montague.dtb montaguep.dtb
montague-hsp-overlay.dtbo-base := montague.dtb montaguep.dtb
montague-talyn-overlay.dtbo-base := montague-talyn.dtb montaguep.dtb
montague-moselle-overlay.dtbo-base := montague-4gb.dtb montaguep-4gb.dtb montague.dtb montaguep.dtb
montague-rcm-overlay.dtbo-base := montague.dtb montaguep.dtb
montague-wsa-overlay.dtbo-base := montague.dtb montaguep.dtb
montague-wsd-overlay.dtbo-base := montague.dtb montaguep.dtb
else
dtb-$(CONFIG_ARCH_RAVELIN) += ravelin-rumi.dtb \
	ravelin-atp.dtb \
	ravelin-idp.dtb \
	ravelin-idp-wcn3988-4gb.dtb \
	ravelin-idp-wcn3950-amoled-rcm.dtb \
	ravelin-qrd.dtb \
	ravelin-qrd-4gb.dtb \
	ravelinp-atp.dtb \
	ravelinp-idp.dtb \
	ravelinp-idp-wcn3988-4gb.dtb \
	ravelinp-idp-wcn3950-amoled-rcm.dtb \
	ravelinp-qrd.dtb \
	ravelinp-qrd-4gb.dtb \
	montague-rumi.dtb \
	montague-hsp.dtb \
	montague-talyn.dtb \
	montague-moselle.dtb \
	montague-rcm.dtb \
	montaguep-rumi.dtb \
	montaguep-hsp.dtb \
	montaguep-moselle.dtb \
	montaguep-rcm.dtb \
	montague-moselle-4gb.dtb \
	montaguep-moselle-4gb.dtb \
	montague-wsa.dtb \
	montague-wsd.dtb \
	montaguep-wsa.dtb \
	montaguep-wsd.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_SM8150) += \
                sm8150-cdp-overlay.dtbo \
                sa8155-adp-star-overlay.dtbo \
                sa8155p-adp-star-overlay.dtbo \
                sa8155-v2-adp-air-overlay.dtbo \
                sa8155p-v2-adp-air-overlay.dtbo

sm8150-cdp-overlay.dtbo-base := sm8150.dtb sm8150-v2.dtb sm8150p.dtb sm8150p-v2.dtb
sa8155-adp-star-overlay.dtbo-base := sa8155.dtb sa8155-v2.dtb
sa8155p-adp-star-overlay.dtbo-base := sa8155p.dtb sa8155p-v2.dtb
sa8155-v2-adp-air-overlay.dtbo-base := sa8155.dtb sa8155-v2.dtb
sa8155p-v2-adp-air-overlay.dtbo-base := sa8155p.dtb sa8155p-v2.dtb
else
dtb-$(CONFIG_ARCH_SM8150)       += sm8150-cdp.dtb \
        sa8155-adp-star.dtb \
        sa8155p-adp-star.dtb \
        sa8155-v2-adp-star.dtb \
        sa8155p-v2-adp-star.dtb \
        sa8155-v2-adp-air.dtb \
        sa8155p-v2-adp-air.dtb
endif
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_ANORAK) += anorak-rumi-overlay.dtbo \
		anorak-idp-overlay.dtbo \
		anorak-qxr-overlay.dtbo \
		anorak-atp-overlay.dtbo \
		anorak-idp-hmt-overlay.dtbo \
		anorak-qxr-hmt-ufs-ref-overlay.dtbo \
		anorak-qxr-4kdisp-hmt-ufs-ref-overlay.dtbo \
		anorak-qxr-hmt-overlay.dtbo \
		anorak-idp-sd-overlay.dtbo \
		anorak-idp-top-hmt-overlay.dtbo \
		anorak-idp-top-hsp-overlay.dtbo \
		anorak-idp-4kdisp-hsp-overlay.dtbo \
		anorak-idp-4kdisp-hmt-overlay.dtbo \
		anorak-idp-hsp-nodisplay-overlay.dtbo
anorak-rumi-overlay.dtbo-base := anorak.dtb
anorak-idp-overlay.dtbo-base := anorak.dtb
anorak-qxr-overlay.dtbo-base := anorak.dtb
anorak-atp-overlay.dtbo-base := anorak.dtb
anorak-idp-hmt-overlay.dtbo-base := anorak.dtb
anorak-qxr-hmt-ufs-ref-overlay.dtbo-base := anorak.dtb
anorak-qxr-4kdisp-hmt-ufs-ref-overlay.dtbo-base := anorak.dtb
anorak-qxr-hmt-overlay.dtbo-base := anorak.dtb
anorak-idp-sd-overlay.dtbo-base := anorak.dtb
anorak-idp-top-hmt-overlay.dtbo-base := anorak.dtb
anorak-idp-top-hsp-overlay.dtbo-base := anorak.dtb
anorak-idp-4kdisp-hsp-overlay.dtbo-base := anorak.dtb
anorak-idp-4kdisp-hmt-overlay.dtbo-base := anorak.dtb
anorak-idp-hsp-nodisplay-overlay.dtbo-base := anorak.dtb
else
dtb-$(CONFIG_ARCH_ANORAK) += anorak-rumi.dtb \
		anorak-idp.dtb \
		anorak-qxr.dtb \
		anorak-atp.dtb \
		anorak-idp-hmt.dtb \
		anorak-qxr-hmt-ufs-ref.dtb \
		anorak-qxr-4kdisp-hmt-ufs-ref.dtb \
		anorak-qxr-hmt.dtb \
		anorak-idp-sd.dtb \
		anorak-idp-top-hmt.dtb \
		anorak-idp-top-hsp.dtb \
		anorak-idp-4kdisp-hsp.dtb \
		anorak-idp-4kdisp-hmt.dtb \
		anorak-idp-hsp-nodisplay.dtb
endif

ifeq ($(CONFIG_ARCH_LAHAINA), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) += lahaina-vm-mtp.dtb \
	lahaina-vm-cdp.dtb \
	lahaina-vm-qrd.dtb
endif
endif

dtb-$(CONFIG_ARCH_SDXLEMUR) += sdxlemur-rumi.dtb \
	sdxlemur-cdp.dtb \
	sdxlemur-mtp.dtb

ifeq ($(CONFIG_ARCH_SHIMA), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) += shima-vm-rumi.dtb \
	shima-vm-atp.dtb \
	shima-vm-idp.dtb \
	shima-vm-qrd.dtb
endif
endif

ifeq ($(CONFIG_ARCH_WAIPIO), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) += waipio-vm-mtp.dtb \
	waipio-vm-cdp.dtb \
	waipio-vm-qrd.dtb \
        waipio-vm-rumi.dtb \
	waipio-lte-vm-mtp.dtb \
	waipio-lte-vm-cdp.dtb \
	waipio-lte-vm-qrd.dtb
endif
endif

ifeq ($(CONFIG_ARCH_CAPE), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) += cape-vm-mtp.dtb \
	cape-vm-cdp.dtb \
	cape-vm-cdp-qhd.dtb \
	cape-vm-qrd.dtb \
	ukee-vm-mtp.dtb \
	ukee-vm-cdp.dtb \
	ukee-vm-cdp-qhd.dtb \
	ukee-vm-qrd.dtb
endif
endif

ifeq ($(CONFIG_ARCH_DIWALI), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) +=  \
	diwali-vm-rumi.dtb \
	diwali-vm-atp.dtb \
	diwali-vm-idp.dtb \
	diwali-vm-qrd.dtb
endif
endif

ifeq ($(CONFIG_ARCH_PARROT), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) +=  \
	parrot-vm-rumi.dtb \
	parrot-vm-atp.dtb \
	parrot-vm-idp.dtb \
	parrot-vm-idp-wcn3990.dtb \
	parrot-vm-idp-wcn3990-amoled-rcm.dtb \
	parrot-vm-idp-wcn6750-amoled.dtb \
	parrot-vm-idp-wcn6750-amoled-rcm.dtb \
	parrot-vm-qrd.dtb \
	parrot-vm-qrd-wcn6750.dtb
endif
endif

ifeq ($(CONFIG_ARCH_RAVELIN), y)
ifeq ($(CONFIG_ARCH_QTI_VM), y)
dtb-$(CONFIG_ARCH_QTI_VM) +=  \
	ravelin-vm-rumi.dtb \
	ravelin-vm-atp.dtb \
	ravelin-vm-idp.dtb \
	ravelin-vm-idp-wcn3988.dtb \
	ravelin-vm-idp-wcn3950-amoled-rcm.dtb \
	ravelin-vm-qrd.dtb \
	montague-vm-moselle.dtb \
	montague-vm-hsp.dtb

endif
endif
endif # CONFIG_OCULUS

always-y	:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
