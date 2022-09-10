import gdb
import binascii
import struct

global_trace_information = []
global_heap_information = {}
global_kernel_heap = 0
global_user_heap = 0
global_tracing = False

class TraceStart(gdb.Command):
    def __init__(self):
        super(TraceStart, self).__init__("trace_start", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_trace_information
        global_trace_information = []
        global global_heap_information
        global_heap_information = {}
        global global_tracing
        global_tracing = True
        global global_kernel_heap
        global_kernel_heap = gdb.parse_and_eval('&g_kmmheap')
        global global_user_heap
        global_user_heap = gdb.parse_and_eval('&g_mmheap')
        print('Trace information reset')

TraceStart()

class TraceStop(gdb.Command):
    def __init__(self):
        super(TraceStop, self).__init__("trace_stop", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_tracing
        global_tracing = False
        print('Trace stopped')

TraceStop()

class AddBackTrace(gdb.Command):
    def __init__(self):
        super(AddBackTrace, self).__init__("trace_add_backtrace", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_trace_information
        global global_tracing
        if global_tracing:
            bt = NuttxAndMonoBacktrace()
            global_trace_information.append(bt.backtrace(include_managed_code=True))
            print('Backtrace collected, continuing execution')
        else:
            print('Cannot collect backtrace as tracing is disabled')

AddBackTrace()

class AddHeapTrace(gdb.Command):
    def __init__(self):
        super(AddHeapTrace, self).__init__("trace_add_heaptrace", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_trace_information
        global global_tracing
        if global_tracing:
            bt = NuttxAndMonoBacktrace()
            address = long(gdb.parse_and_eval('ret'))
            heapdata = {}
            heapdata['requested'] = long(gdb.parse_and_eval('size'))
            heapdata['allocated'] = long(gdb.parse_and_eval('alignsize'))
            heapdata['heap'] = long(gdb.parse_and_eval('heap'))
            heapdata['backtrace'] = bt.backtrace(include_managed_code=False)
            global_heap_information[address] = heapdata
        else:
            print('Cannot collect heap information as tracing is disabled')

AddHeapTrace()

class RemoveHeapTrace(gdb.Command):
    def __init__(self):
        super(RemoveHeapTrace, self).__init__("trace_remove_heaptrace", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_heap_information
        global global_tracing
        if global_tracing:
            address = long(gdb.parse_and_eval("mem"))
            if address in global_heap_information.keys():
                del global_heap_information[address]
            else:
                raise Exception('Cannot find memory allocation for address 0x%0.8x' % address)


RemoveHeapTrace()

class ShowTraceData(gdb.Command):
    def __init__(self):
        super(ShowTraceData, self).__init__("show trace_data", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_trace_information
        trace_count = 1
        for backtrace in global_trace_information:
            if trace_count != 1:
                print('\n')
            print('********** Trace %d' % trace_count)
            trace_count += 1
            for line in backtrace:
                print(line)

ShowTraceData()
class ShowHeapTraceData(gdb.Command):
    def __init__(self):
        super(ShowHeapTraceData, self).__init__("show heap_trace", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_heap_information
        global global_kernel_heap
        for heapdata in global_heap_information:
            heapinfo = global_heap_information[heapdata]
            if heapinfo['heap'] == global_kernel_heap:
                heap = 'kernel'
            else:
                heap = 'user'
            print('Memory allocation 0x%0.8x, requested %d, allocated %d from %s heap' % (heapdata, heapinfo['requested'], heapinfo['allocated'], heap))
            for line in heapinfo['backtrace']:
                print('    %s' %line)

ShowHeapTraceData()
