import gdb
import binascii
import struct

g_trace_information = None
g_heap_information = None
g_heap_unknown_frees = None
g_kernel_heap = 0
g_user_heap = 0
g_tracing = False
g_ignored_heap_methods = [ 'malloc', 'mm_malloc', 'mm_zalloc', 'mm_calloc']

class TraceStart(gdb.Command):
    def __init__(self):
        super(TraceStart, self).__init__("trace_start", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global g_trace_information
        g_trace_information = []
        global g_heap_information
        g_heap_information = {}
        global g_heap_unknown_frees
        g_heap_unknown_frees = []
        global g_tracing
        g_tracing = True
        global g_kernel_heap
        g_kernel_heap = gdb.parse_and_eval('&g_kmmheap')
        global g_user_heap
        g_user_heap = gdb.parse_and_eval('&g_mmheap')
        print('Trace information reset')

TraceStart()

class TraceStop(gdb.Command):
    def __init__(self):
        super(TraceStop, self).__init__("trace_stop", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global g_tracing
        g_tracing = False
        print('Trace stopped')

TraceStop()

class AddBackTrace(gdb.Command):
    def __init__(self):
        super(AddBackTrace, self).__init__("trace_add_backtrace", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global g_trace_information
        global g_tracing
        if g_tracing:
            bt = NuttxAndMonoBacktrace()
            g_trace_information.append(bt.backtrace(include_managed_code=True))
            print('Backtrace collected, continuing execution')
        else:
            print('Cannot collect backtrace as tracing is disabled')

AddBackTrace()

class AddHeapTrace(gdb.Command):
    def __init__(self):
        super(AddHeapTrace, self).__init__("trace_add_heaptrace", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global g_trace_information
        global g_tracing
        if g_tracing:
            bt = NuttxAndMonoBacktrace()
            address = int(gdb.parse_and_eval('ret'))
            heapdata = {}
            heapdata['requested'] = int(gdb.parse_and_eval('size'))
            heapdata['allocated'] = int(gdb.parse_and_eval('alignsize'))
            heapdata['heap'] = int(gdb.parse_and_eval('heap'))
            heapdata['backtrace'] = bt.backtrace(include_managed_code=False)
            g_heap_information[address] = heapdata
        else:
            print('Cannot collect heap information as tracing is disabled')

AddHeapTrace()

class RemoveHeapTrace(gdb.Command):
    def __init__(self):
        super(RemoveHeapTrace, self).__init__("trace_remove_heaptrace", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global g_heap_information
        global g_tracing
        if g_tracing:
            address = int(gdb.parse_and_eval("mem"))
            if address in list(g_heap_information.keys()):
                del g_heap_information[address]
            else:
                heap = int(gdb.parse_and_eval('heap'))
                global g_heap_unknown_frees
                free = {}
                free['address'] = address
                free['heap'] = heap
                g_heap_unknown_frees.append(free)
                # raise Exception('Cannot find memory allocation for address 0x%0.8x in %s heap' % (address, heapname(heap)))

RemoveHeapTrace()

class ShowTraceData(gdb.Command):
    def __init__(self):
        super(ShowTraceData, self).__init__("show trace_data", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global g_trace_information
        trace_count = 1
        for backtrace in g_trace_information:
            if trace_count != 1:
                print('\n')
            print('********** Trace %d' % trace_count)
            trace_count += 1
            for line in backtrace:
                print(line)

ShowTraceData()

# class ShowHeapTraceData(gdb.Command):
#     def __init__(self):
#         super(ShowHeapTraceData, self).__init__("show heap_trace", gdb.COMMAND_STACK)

#     def invoke(self, arg, from_tty):
#         global g_heap_information
#         global g_kernel_heap
#         global heapname
#         for heapdata in g_heap_information:
#             heapinfo = g_heap_information[heapdata]
#             print('Memory allocation 0x%0.8x, requested %d, allocated %d from %s heap' % (heapdata, heapinfo['requested'], heapinfo['allocated'], heapname(heapinfo['heap'])))
#             for line in heapinfo['backtrace']:
#                 if not [method for method in g_ignored_heap_methods if method in line]:
#                     print('    %s' % line)
#         if len(g_heap_unknown_frees) > 0:
#             print('Free from unknown addresses:')
#             for unknown_free in g_heap_unknown_frees:
#                 heap = unknown_free['heap']
#                 address = unknown_free['address']
#                 print('    Address: 0x%0.8x on %s heap' % (address, heapname(heap)))

# ShowHeapTraceData()

def heapname(address):
    global g_kernel_heap
    if address == g_kernel_heap:
        name = 'kernel'
    else:
        name = 'user'
    return name
