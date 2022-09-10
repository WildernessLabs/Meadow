import gdb
import binascii
import struct

global_trace_information = []
global_heap_information = {}
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
            address = long(gdb.parse_and_eval("ret"))
            global_heap_information[address] = bt.backtrace(include_managed_code=False)
        else:
            print('Cannot collect heap information as tracing is disabled')

AddHeapTrace()

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
        for heapdata in global_heap_information:
            print('Memory allocation 0x%0.8x' % heapdata)
            for line in global_heap_information[heapdata]:
                print('    %s' %line)

ShowHeapTraceData()
