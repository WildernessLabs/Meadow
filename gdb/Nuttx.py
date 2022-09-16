import gdb
import binascii
import struct

verbose = False
is_qemu = False
is_protected_build = True

# This receives a base stack pointer and reads the register
# values saved in memory by the ARM processor and NuttX.
# See arch/arm/src/armv7-m/gnu/up_lazyexception.S for details.

class NuttxRegContext():
    def __init__(self, stack_top, skip_fpu = False):
        self.stack_top = stack_top

        self.all_regs = [ "r0","r1","r2","r3","r4","r5","r6", "r7","r8","r9",
            "r10","r11","r12", "sp","lr","pc","xpsr" ]
            # ,"msp","psp","control","faultmask","basepri","primask","fpscr"

        self.sw_regs = ["r2","r3","r4","r5","r6","r7","r8","r9","r10","r11","r14"]        
        self.sw_regs_base = self.stack_top + len(self.sw_regs) * 4

        self.fpu_regs = [None] * 33
        if skip_fpu == True:
            self.fpu_regs_base = self.sw_regs_base
        else:
            self.fpu_regs_base = self.sw_regs_base + len(self.fpu_regs) * 4

        self.hw_regs = ["r0","r1","r2","r3","r12","lr","pc","xpsr"]
        self.hw_regs_base = self.fpu_regs_base + len(self.hw_regs) * 4

    def debug_registers(self):
        print("stack_top %s sw_regs_base %s fpu_regs_base %s hw_regs_base %s" 
            % (self.stack_top, self.sw_regs_base, self.fpu_regs_base,
                self.hw_regs_base))

    def read_registers(self):
        sw_regs_values = [self.read_sw_register(reg)[0] for reg in self.sw_regs]
        sw_regs = dict(zip(self.sw_regs, sw_regs_values))

        hw_regs_values = [self.read_hw_register(reg)[0] for reg in self.hw_regs]
        hw_regs = dict(zip(self.hw_regs, hw_regs_values))

        regs = dict(sw_regs.items() + hw_regs.items())
        return regs

    def dump_hw_registers(self):
        values = [self.read_hw_register(reg)[0] for reg in self.hw_regs]
        regs = dict(zip(self.hw_regs, values))
        for reg in self.hw_regs:
            key = "r14" if reg == "sp" else reg
            print("%s\t\t0x%s" % (reg, regs[key]))

    def dump_sw_registers(self):
        values = [self.read_sw_register(reg)[0] for reg in self.sw_regs]
        regs = dict(zip(self.sw_regs, values))
        for reg in self.sw_regs:
            key = "r14" if reg == "sp" else reg
            print("%s\t\t0x%s" % (reg, regs[key]))

    def dump_registers(self):
        regs = self.read_registers()
        for reg in self.all_regs:
            key = "r14" if reg == "sp" else reg
            print("%s\t\t0x%s" % (reg, regs[key]))

    def read_hw_register(self, reg, base = 0):
        if base == 0:
            base = self.hw_regs_base
        i = self.hw_regs.index(reg)
        return read_memory_word_offset(base, len(self.hw_regs) - i)

    def read_sw_register(self, reg):
        i = self.sw_regs.index(reg)
        return read_memory_word_offset(self.sw_regs_base, len(self.sw_regs) - i)

# This keeps a saved copy of the ARM processor registers state.
class ARMRegContext():
    def __init__(self, frame):
        self.frame = frame
        self.openocd_regs = [ "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",
                      "r9", "r10", "r11", "r12", "sp", "lr", "pc", "xPSR",
                      "msp", "psp", "control", "faultmask", "basepri",
                      "primask", "fpscr" ]
        self.qemu_regs = [ "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8",
                      "r9", "r10", "r11", "r12", "sp", "lr", "pc", "cpsr",
                      "fpscr", "fpsid", "fpexc" ]
        self.regs = self.qemu_regs if is_qemu else self.openocd_regs
        self.values = [None] * len(self.regs)

    # Saves the current values of the registers.
    def save_registers(self):
        for i,reg in enumerate(self.regs):
            value = long(self.frame.read_register(reg))
            self.values[i] = value

    # Restores the previously saved values of the registers.
    def restore_registers(self):
        for i,reg in enumerate(self.regs):
            cmd = "$%s = %s" % (reg, self.values[i])
            gdb.parse_and_eval("$%s = %s" % (reg, self.values[i]))

    def dump_registers(self):
        for i,reg in enumerate(self.regs):
            print("%s: %s" % (reg, self.values[i]))

class NuttxAndMonoBacktrace():
    def __init__(self):
        self._backtrace = []

    def backtrace(self, include_managed_code):
        try:
            self._backtrace = []
            # Save a copy of the current CPU context.
            self.ctx = ARMRegContext(gdb.newest_frame())
            self.ctx.save_registers()

            frame = gdb.selected_frame()

            i = 0
            while frame != None:
                if frame.pc() == 0:
                    break

                annotations = self.annotate_frame(frame)

                if frame.name() == "interp_exec_method_full":
                    if include_managed_code:
                        s = self.managed_frame(frame, i, annotations)
                        self._backtrace.append(s)
                else:
                    s = self.frame(frame, i, annotations)
                    self._backtrace.append(s)

                self.handle_frame(frame)
                i = i + 1

                # This can happen when we switch CPU context.
                if not frame.is_valid():
                    frame = gdb.selected_frame()
                else:
                    frame = frame.older()
        finally:
            self.ctx.restore_registers()
        return self._backtrace

    def get_managed_frame_name(self, frame):
        frame_var_addr = int(frame.read_var("frame"))
        method_expr = "((struct _InterpFrame*) 0x%s).imethod->method" % format_hex(frame_var_addr)
        managed = gdb.parse_and_eval("mono_method_full_name(%s, 1)" % method_expr)
        managed = str(managed).translate(None, '"')
        return managed

    def managed_frame(self, frame, i, annotations):
        sal = frame.find_sal()
        result = "#%s %s [%s]" % (i, self.get_managed_frame_name(frame), annotations)
        return(result)

    def frame(self, frame, i, annotations):
        annotations_text = ""
        if annotations != None:
            annotations_text = "[%s]" % (annotations)

        sal = frame.find_sal()
        symtab = sal.symtab
        filename = symtab.filename if symtab else ""
        result = "#%s 0x%s %s () at %s:%s %s" % (i, format_hex(sal.pc), frame.name(), filename, sal.line, annotations_text)
        return(result)

    def annotate_frame(self, frame):
        if frame.name() == None:
            return
        elif frame.name() == "up_svcall":
            return self.annotate_frame_svcall(frame)
        elif frame.name() == "exception_common":
            return self.annotate_frame_exception_common(frame)
        elif frame.name() == "interp_exec_method_full":
            return self.annotate_frame_interp(frame)
        elif "sys_call" in frame.name():
            return self.annotate_frame_sys_call(frame)

    def annotate_frame_interp(self, frame):
        return "managed"

    def annotate_frame_sys_call(self, frame):
        return "syscall"

    def annotate_frame_exception_common(self, frame):
            reg = "cpsr" if is_qemu else "xPSR"
            xpsr = frame.read_register(reg)
            ipsr = long(xpsr & 0x0000001f)
            stm_vectors = [ "IDLE_STACK", "__start", "stm32_nmi",
                "stm32_hardfault", "stm32_mpu", "stm32_busfault",
                "stm32_usagefaulf", "stm32_reserved", "stm32_reserved",
                "stm32_reserved", "stm32_reserved", "stm32_svcall",
                "stm32_dbgmonitor", "stm32_reserved", "stm32_pendsv",
                "stm32_systick"]
            #assert ipsr < len(stm_vectors)
            if ipsr < len(stm_vectors):
                return stm_vectors[ipsr]
            else:
                print("Cannot lookup exception vector with ipsr value %s" % (ipsr))
            return

    def annotate_frame_svcall(self, frame):
            regs = frame.read_var("context")
            ctx = NuttxRegContext(regs)
            cmd = long(ctx.read_hw_register("r0")[0])
            svcalls = ["SYS_save_context", "SYS_restore_context",
                       "SYS_switch_context", "SYS_syscall_return",
                       "SYS_task_start", "SYS_pthread_start",
                       "SYS_signal_handler", "SYS_signal_handler_return" ]
            if  cmd < len(svcalls):
                return svcalls[cmd]
            return
            # TODO: Add annotating support for non-core syscalls.

    def handle_frame(self, frame):
        if frame.name() == "exception_common":
            self.handle_frame_exception_common(frame)
        elif frame.name() == "dispatch_syscall":
            self.handle_frame_dispatch_syscall(frame)

    def handle_frame_dispatch_syscall(self, frame):
        # See default case of up_svcall.
        # It sets up the original frame return in the TCB xcp regs structure.
        # TODO: Handle CONFIG_SMP build if we support it in the future.
        nsyscalls = long(gdb.parse_and_eval(
            "((struct tcb_s *)g_readytorun.head)->xcp.nsyscalls"))

        CONFIG_SYS_NNEST = 2
        assert nsyscalls <= CONFIG_SYS_NNEST

        index = nsyscalls - 1
        sysreturn = long(gdb.parse_and_eval(
            "((struct tcb_s *)g_readytorun.head)->xcp.syscall[%d].sysreturn"
                % index))

        pc = sysreturn

        # How we get the LR value depends on where exactly we are stopped
        # inside dispatch_syscall. It can be saved on the stack or in the
        # LR register if we have not branched to the syscall stub yet.
        if frame.newer() != None:
            sp = frame.read_register("sp")
            lr =  read_memory_word(long(sp) + 12)
            sp = sp + 16
        else:
            # Need to take into account PC relative to dispatch_syscall
            print("Not yet implemented")

        #print("set $sp = 0x%s" % format_hex(sp))
        #print("set $lr = 0x%s" % lr)
        #print("set $pc = 0x%s" % format_hex(pc))

        gdb.parse_and_eval("$sp = 0x%s" % format_hex(sp))
        gdb.parse_and_eval("$lr = 0x%s" % lr)
        gdb.parse_and_eval("$pc = 0x%s" % format_hex(pc))

    def handle_frame_exception_common(self, frame):
        r4 = frame.read_register("r4")
        #  Get the stack pointer before exception handler.
        #  (8 HW regs) + (33 FPU regs) + (10/11 SW regs) * 4 bytes
        num_sw_regs = 11 if is_protected_build else 10
        ctx_size = (8 + 33 + num_sw_regs) * 4
        ### IRQ Stack Frame Format
        # SW_REGS (8)
        # REG_R0              (SW_XCPT_REGS+0) /* R0 */
        # REG_R1              (SW_XCPT_REGS+1) /* R1 */
        # REG_R2              (SW_XCPT_REGS+2) /* R2 */
        # REG_R3              (SW_XCPT_REGS+3) /* R3 */
        # REG_R12             (SW_XCPT_REGS+4) /* R12 */
        # REG_R14             (SW_XCPT_REGS+5) /* R14 = LR *
        # REG_R15             (SW_XCPT_REGS+6) /* R15 = PC *
        # REG_XPSR            (SW_XCPT_REGS+7) /* xPSR */
        #
        # FPU_REGS (33)
        #
        # HW_REGS (10 or 11)
        # REG_R13             (0)  /* R13 = SP at time of interrupt */
        # REG_PRIMASK         (1)  /* PRIMASK */
        # REG_R4              (2)  /* R4 */
        # REG_R5              (3)  /* R5 */
        # REG_R6              (4)  /* R6 */
        # REG_R7              (5)  /* R7 */
        # REG_R8              (6)  /* R8 */
        # REG_R9              (7)  /* R9 */
        # REG_R10             (8)  /* R10 */
        # REG_R11             (9)  /* R11 */
        # REG_EXC_RETURN      (10) /* EXC_RETURN / if protected build mode */ 
        ###
        ctx = NuttxRegContext(r4)
        ctx.sw_regs = ["r13","primask","r4","r5","r6","r7","r8","r9","r10","r11"] \
            + ["r14"] * is_protected_build
        user_sp = ctx.read_sw_register("r13")[0]
        user_pc = ctx.read_hw_register("pc")[0]
        user_lr = ctx.read_hw_register("lr")[0]
        #print("set $sp = 0x%s" % user_sp)
        #print("set $lr = 0x%s" % user_lr)
        #print("set $pc = 0x%s" % user_pc)
        gdb.parse_and_eval("$sp = 0x%s" % (user_sp))
        gdb.parse_and_eval("$lr = 0x%s" % (user_lr))
        gdb.parse_and_eval("$pc = 0x%s" % (user_pc))

global_saved_regs_ctx = None

class NuttxBacktrace(gdb.Command):
    def __init__(self):
        super(NuttxBacktrace, self).__init__("nx_bt", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        nuttx_and_mono_backtrace = NuttxAndMonoBacktrace()
        for line in nuttx_and_mono_backtrace.backtrace(include_managed_code=True):
            print(line)

class NuttxDumpRegisters(gdb.Command):
    def __init__(self):
        super(NuttxDumpRegisters, self).__init__("nx_dumpregs",
            gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        addr = gdb.Value(long(arg, 0))
        ctx = NuttxRegContext(addr)
        ctx.dump_registers()

NuttxDumpRegisters()

class NuttxDumpHWRegisters(gdb.Command):
    def __init__(self):
        super(NuttxDumpHWRegisters, self).__init__("nx_dumpregs_hw",
            gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        addr = gdb.Value(long(arg, 0))
        ctx = NuttxRegContext(addr, True)
        ctx.dump_hw_registers()

NuttxDumpHWRegisters()

class NuttxSaveRegisters(gdb.Command):
    def __init__(self):
        super(NuttxSaveRegisters, self).__init__("nx_saveregs",
            gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_saved_regs_ctx
        print("Saving CPU registers context...")
        global_saved_regs_ctx = ARMRegContext(gdb.selected_frame())
        global_saved_regs_ctx.save_registers()

class NuttxRestoreRegisters(gdb.Command):
    def __init__(self):
        super(NuttxRestoreRegisters, self).__init__("nx_restoreregs",
            gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        global global_saved_regs_ctx
        if global_saved_regs_ctx == None:
            print("No saved CPU registers context was found.")
            return
        print("Restoring CPU registers context...")
        global_saved_regs_ctx.restore_registers()

def read_memory_word(addr):
    inferior = gdb.selected_inferior()
    mem = inferior.read_memory(addr, 4)
    return format_hex_swap32(mem)

def read_memory_word_offset(base, offset):
    addr = base - offset * 4
    inferior = gdb.selected_inferior()
    mem = inferior.read_memory(addr, 4)
    return format_hex_swap32(mem), addr

def format_hex(i):
    return binascii.hexlify(struct.pack(">I", i))

def format_hex_swap32(i):
    return binascii.hexlify(struct.pack("<I", struct.unpack(">I", i)[0]))

NuttxBacktrace()
NuttxSaveRegisters()
NuttxRestoreRegisters()