# ACCFIX for MT6577

1. Copy files into your kernel project dir.
2. Change drivers config in mediatek/platform/mt6577/kernel/Kconfig/Drivers like
    # Accdet
    config MTK_ACCDET # ACCDET
            tristate "MediaTek Accessory Detection Support"
            default m
3. Change kernel config in mediatek/config/<project>/autoconfig/kconfig/project like
    CONFIG_MTK_ACCDET=m
4. Build module
    ./makeMtk <project> r k mediatek/platform/mt6577/kernel/drivers/accdet