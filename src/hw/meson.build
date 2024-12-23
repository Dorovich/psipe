pnvl_ss = ss.source_set()
pnvl_ss.add(files(
    'dma.c',
    'irq.c',
    'mmio.c',
    'proxy.c',
    'pnvl.c',
))

system_ss.add_all(when: 'CONFIG_PNVL', if_true: pnvl_ss)
