import simics
import simics_common
from comp import *

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

    class mem_size(SimpleConfigAttribute(0x10000000, 'i')):
        """Contoller RAM size, default is 0x10000000 bytes."""
        def setter(self, val):
            if val <= 0:
                raise CompException('Illegal controller memory size %d' % val)
            self.val = val

    class component_queue(StandardComponent.component_queue):
        def getter(self):
            return self._up.get_clock()

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
        cpu_core.physical_memory_space = phys_mem
        mem = self.add_pre_obj('ram', 'ram')
        mem_image = self.add_pre_obj('mem_image', 'image')
        mem_image.size = self.mem_size.val
        mem.image = mem_image
        phys_mem.map = [[0x00000000, mem, 0, 0, self.mem_size.val]]

    def get_processors(self):
        ret = [self.get_slot('cpu')]
        return list(set(ret))

    class top_level(StandardComponent.top_level):
        def _initialize(self):
            self.val = True

    class cpu_list(StandardComponent.cpu_list):
        def getter(self):
            return self._up.get_processors()
