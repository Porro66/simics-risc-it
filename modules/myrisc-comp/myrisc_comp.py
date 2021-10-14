import simics
import simics_common
from comp import *

DEF_RAM_SIZE = 0x400000
DEF_IOR_SIZE = 0x10000
DEF_ROM_SIZE = 0x40000
TOTAL_PROTECTED_AREAS = 2

class RISC_controller(StandardConnectorComponent):
    """Base class for RISC controller."""

    def setup(self):
        super().setup()
        if not self.instantiated.val:
            self.add_objects()

    class basename(StandardConnectorComponent.basename):
        val = "controller"

    class freq_mhz(SimpleConfigAttribute(40, 'i')):
        """Processor frequency in MHz, default is 40 MHz."""
        def setter(self, val):
            if val <= 0:
                raise CompException('Illegal processor frequency %d' % val)
            self.val = val

    class mem_size(SimpleConfigAttribute(DEF_RAM_SIZE, 'i')):
        """Contoller RAM size, default is 0x%x bytes)""" % DEF_RAM_SIZE
        def setter(self, val):
            if val <= 0:
                raise CompException('Illegal controller memory size %d' % val)
            self.val = val

    class firmware(SimpleConfigAttribute('', 's')):
        """Controller's firmware file to use."""
        def lookup(self):
            if self.val:
                lookup = simics.SIM_lookup_file(self.val)
                if not lookup:
                    print('lookup of firmware files %s failed' % self.val)
                    return ''
                return lookup
            return self.val

    class component_queue(StandardComponent.component_queue):
        def getter(self):
            return self._up.get_clock()

    class component(StandardComponent.component):
        def pre_instantiate(self):
            return self._up.pre_instantiate_controller()
        def post_instantiate(self):
            self._up.post_instantiate_controller()
    
    def pre_instantiate_controller(self):
        return True

    def post_instantiate_controller(self):
        self.init_firmware();

    def get_clock(self):
        sub_cmps = [x for x in self.obj.iface.component.get_slot_objects() if (
            isinstance(x, simics.conf_object_t) and hasattr(x.iface, 'component'))]
        clocks = []
        for c in sub_cmps:
            if hasattr(c, 'component_queue'):
                q = c.component_queue
                if q:
                    clocks.append(q)
        if len(clocks) > 0:
            return clocks[0]

    def add_objects(self):
        cpu_core = self.add_pre_obj('cpu_core', 'sample-risc-core')
        cpu = self.add_pre_obj('cpu', 'sample-risc')
        cpu.freq_mhz = self.freq_mhz.val
        cpu.current_risc_core = cpu_core
        cpu_core.sample_risc = cpu
        phys_mem = self.add_pre_obj('phys_mem', 'memory-space')
        phys_mem.map = []
        cpu_core.physical_memory_space = phys_mem
      
        unmapped_ff = self.add_pre_obj('unmapped_ff', 'set-memory')
        mem_mng = self.add_pre_obj('mem_mng', 'mem_mng') # "memory controller"
        ram_space = self.add_pre_obj('ram_space', 'memory-space')
        mem_image = self.add_pre_obj('mem_image', 'image')
        mem_image.size = self.mem_size.val
        mem = self.add_pre_obj('ram', 'ram')
        mem.image = mem_image
        mem_space = self.add_pre_obj('mem_space', 'memory-space')
        mem_space.map = [[0, mem, 0, 0, self.mem_size.val]]
        mem_mng.mem_tgt = mem_space
        mem_mng.unmapped_ff = unmapped_ff
        ram_space.default_target = [[mem_mng, 'mem_decoder'], 0, 0, mem_space]
        phys_mem.map += [[0x0, [mem_mng, "mmng"], 0, 0, TOTAL_PROTECTED_AREAS * 3 * 4, None, 0, 4],
                         [DEF_IOR_SIZE+DEF_ROM_SIZE, ram_space, 0, 0, self.mem_size.val]
                        ]

        # Firmware
        if self.firmware.lookup():
            rom = self.add_pre_obj('rom', 'rom')
            rom_image = self.add_pre_obj('rom_image', 'image')
            rom_image.size = DEF_ROM_SIZE
            rom.image = rom_image
            phys_mem.map += [[DEF_IOR_SIZE, rom, 0, 0, DEF_ROM_SIZE]]

    def init_firmware(self):
        if self.firmware.lookup():
            # Load the firmware into the ROM area
            simics.SIM_load_file(self.get_slot('phys_mem'), self.firmware.val,
                                 DEF_IOR_SIZE, True)

    def get_processors(self):
        ret = [self.get_slot('cpu')]
        return list(set(ret))

    class top_level(StandardComponent.top_level):
        def _initialize(self):
            self.val = True

    class cpu_list(StandardComponent.cpu_list):
        def getter(self):
            return self._up.get_processors()
