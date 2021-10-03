# This Software is part of Simics. The rights to copy, distribute,
# modify, or otherwise make use of this Software may be licensed only
# pursuant to the terms of an applicable license agreement.
# 
# Copyright 2010-2021 Intel Corporation

# <add id="sample_components.py" label="sample_components.py">
# <insert-until text="# END sample_components.py"/>
# </add>
import simics
from comp import StandardComponent, SimpleConfigAttribute, Interface

class sample_pci_card(StandardComponent):
    """A sample component containing a sample PCI device."""
    _class_desc = "sample PCI card"
    _help_categories = ('PCI',)

    def setup(self):
        super().setup()
        if not self.instantiated.val:
            self.add_objects()
        self.add_connectors()

    def add_objects(self):
        sd = self.add_pre_obj('sample_dev', 'sample_pci_device')
        sd.int_attr = self.integer_attribute.val

    def add_connectors(self):
        self.add_connector(slot = 'pci_bus', type = 'pci-bus',
                           hotpluggable = True, required = False, multi = False,
                           direction = simics.Sim_Connector_Direction_Up)

    class basename(StandardComponent.basename):
        """The default name for the created component"""
        val = "sample_cmp"

    class integer_attribute(SimpleConfigAttribute(None, 'i',
                                                  simics.Sim_Attr_Required)):
        """Example integer attribute."""

    class component_connector(Interface):
        """Uses connector for handling connections between components."""
        def get_check_data(self, cnt):
            return []
        def get_connect_data(self, cnt):
            return [[[0, self._up.get_slot('sample_dev')]]]
        def check(self, cnt, attr):
            return True
        def connect(self, cnt, attr):
            self._up.get_slot('sample_dev').pci_bus = attr[1]
        def disconnect(self, cnt):
            self._up.get_slot('sample_dev').pci_bus = None

class RISC_controller(StandardComponent):
    """Base class for RISC controller."""

    def setup(self):
        super().setup()
        if not self.instantiated.val:
            self.add_objects()

    class basename(StandardComponent.basename):
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

    def add_objects(self):
        cpu_core = self.add_pre_obj('cpu_core', 'sample-risc-core')
        cpu = simics_common.pre_conf_object('cpu', 'sample-risc')
        cpu.freq_mhz = self.freq_mhz
        cpu.current_risc_core = cpu_core
        phys_mem = simics_common.pre_conf_object('phys_mem', 'memory-space')
        cpu_core.physical_memory_space = phys_mem
        mem = simics_common.pre_conf_object('ram', 'ram')
        mem_image = simics_common.pre_conf_object('mem_image', 'image')
        mem_image.size = self.mem_size
        mem.image = mem_image
        phys_mem.map = [[0x00000000, mem, 0, 0, MEMORY_SIZE]]
        cpu_core.sample_risc = cpu

# END sample_components.py
