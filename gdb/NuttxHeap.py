import gdb
import binascii
import struct

class NuttXHeap ():
    """(NuttX) Collect the heap information for a specified heap."""

    def __init__(self, heap_name):
        '''
        Initialise a NuttXHeap class.

        :param heap_name: Name of the heap to examine (user or kernel), default user.
        :raises gdb.GdbError: Raised if the allocated node size is invalid.
        '''
        struct_mm_allocnode_s = gdb.lookup_type('struct mm_allocnode_s')
        preceding_size = struct_mm_allocnode_s['preceding'].type.sizeof
        if preceding_size == 2:
            self._allocflag = 0x8000
        elif preceding_size == 4:
            self._allocflag = 0x80000000
        else:
            raise gdb.GdbError('invalid mm_allocnode_s.preceding size %u' % preceding_size)
        self._allocnodesize = struct_mm_allocnode_s.sizeof
        self._heap_name = heap_name
        self._heap_information = []

    def _node_allocated(self, allocnode):
        '''
        Is the node allocated?

        :param allocnode: Node to check.
        :return: True if the node is allocated, False otherwise.
        '''
        if allocnode['preceding'] & self._allocflag:
            return True
        return False

    def _node_size(self, allocnode):
        '''
        Extract the size of the node.

        Nodes use the top bit of the size to indicate if the node is a free node or if it has been allocated.

        :param allocnode: Node to get the sizeof.
        :return: Size of the node.
        '''
        return allocnode['size'] & ~self._allocflag

    def _parse_allocations(self, region_start, region_end):
        '''
        Parse all of the nodes in a specific region
        
        This method walks through the memory specified by the region_start and region_end
        parameters.

        The heap information is put into the _heap_information class variable.

        :param region_start: Address of the start of the region.
        :param region_end: Last address in the region.
        :raises gdb.GdbError: Raised if the start of the region is greater than the end of the region. 
        '''
        if region_start >= region_end:
            raise gdb.GdbError('heap region {} corrupt'.format(hex(region_start)))
        nodecount = region_end - region_start
        self._heap_information.append('heap {} - {}'.format(region_start, region_end))
        cursor = 1
        while cursor < nodecount:
            allocnode = region_start[cursor]
            if self._node_allocated(allocnode):
                state = ''
            else:
                state = '(free)'
            s = '  {} {} {}'.format(allocnode.address + self._allocnodesize,
                                                  self._node_size(allocnode), state)
            self._heap_information.append(s)
            cursor += self._node_size(allocnode) / self._allocnodesize

    def get_heap_allocations(self):
        '''
        Get the heap allocations from all of the heap regions in the selected heap.

        The heap is specified in the construction of this class.  The self._heap_name variable
        contains the name of the NuttX variable containing the heap structures.  This will
        normally be one of g_mmheap or g_kmmheap.

        :return: List of string containing the heap allocation information.
        '''
        self._heap_information = []
        heap = gdb.lookup_global_symbol(self._heap_name).value()
        nregions = heap['mm_nregions']
        region_starts = heap['mm_heapstart']
        region_ends = heap['mm_heapend']
        self._heap_information.append('{} heap(s)'.format(nregions))
        # walk the heaps
        if nregions > 0:
            for i in range(0, nregions):
                if region_starts[i] != 0:
                    self._parse_allocations(region_starts[i], region_ends[i])
        return self._heap_information

class NX_show_heap(gdb.Command):
    """
    (NuttX) GDB command to display the list of allocated nodes from the requested heap.

    Usage: show heap [user | kernel]

    If the heap name is not specified then the user heap will be used.
    """

    def __init__(self):
        '''
        Initialise an instance of the NX_show_heap class.
        '''
        super(NX_show_heap, self).__init__("show heap", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        '''
        Execute the 'show heap' command.

        If no argument is specified then the user heap will be assumed.

        :param arg: Arguments from the show heap command.  Normally user or kernel.
        '''
        if arg is None:
            arg = 'user'
        if arg == 'kernel':
            heap_variable_name = 'g_kmmheap'
            heap_name = 'kernel'
        else:
            heap_variable_name = 'g_mmheap'
            heap_name = 'user'
        heap_information = NuttXHeap(heap_variable_name)
        print('Showing heap allocations for %s (%s)' % (heap_name, heap_variable_name))
        if heap_information is not None:
            for line in heap_information.get_heap_allocations():
                print(line)
        else:
            print('No heap information found.')

NX_show_heap()

# class NX_show_free_heap(gdb.Command):
#     """(NuttX) prints the heap"""

#     def __init__(self):
#         super(NX_show_heap, self).__init__('show freeheap', gdb.COMMAND_USER)
#         struct_mm_freenode_s = gdb.lookup_type('struct mm_freenode_s')
#         self._freenodesize = struct_mm_freenode_s.sizeof

#     def _print_allocations(self, region_start, region_end):
#         if region_start >= region_end:
#             raise gdb.GdbError('heap region {} corrupt'.format(hex(region_start)))
#         nodecount = region_end - region_start
#         print ('heap {} - {}'.format(region_start, region_end))
#         cursor = 1
#         while cursor < nodecount:
#             allocnode = region_start[cursor]
#             print( '  {} {} {}'.format(allocnode.address + self._allocnodesize,
#                                                   self._node_size(allocnode), state))
#             cursor += self._node_size(allocnode) / self._allocnodesize

#     def invoke(self, args, from_tty):
#         heap = gdb.lookup_global_symbol('g_mmheap').value()
#         nregions = heap['mm_nregions']
#         region_starts = heap['mm_heapstart']
#         region_ends = heap['mm_heapend']
#         print( '{} heap(s)'.format(nregions))
#         for i in range(0, nregions):
#             self._print_allocations(region_starts[i], region_ends[i])

# NX_show_free_heap()
