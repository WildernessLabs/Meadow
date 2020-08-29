import gdb
import binascii
import struct
#import Nuttx_Tasks

verbose = False
is_qemu = False
is_protected_build = True

# This receives a base stack pointer and reads the register
# values saved in memory by the ARM processor and NuttX.
# See arch/arm/src/armv7-m/gnu/up_lazyexception.S for details.

class NX_task(object):
	"""Reference to a NuttX task and methods for introspecting it"""

	def __init__(self, tcb_ptr):
		self._tcb = tcb_ptr.dereference()
		self._group = self._tcb['group'].dereference()
		self.pid = tcb_ptr['pid']

	@classmethod
	def for_tcb(cls, tcb):
		"""return a task with the given TCB pointer"""
		pidhash_sym = gdb.lookup_global_symbol('g_pidhash')
		pidhash_value = pidhash_sym.value()
		pidhash_type = pidhash_sym.type
		for i in range(pidhash_type.range()[0],pidhash_type.range()[1]):
			pidhash_entry = pidhash_value[i]
			if pidhash_entry['tcb'] == tcb:
				return cls(pidhash_entry['tcb'])
		return None

	@classmethod
	def for_pid(cls, pid):
		"""return a task for the given PID"""
		pidhash_sym = gdb.lookup_global_symbol('g_pidhash')
		pidhash_value = pidhash_sym.value()
		pidhash_type = pidhash_sym.type
		for i in range(pidhash_type.range()[0],pidhash_type.range()[1]):
			pidhash_entry = pidhash_value[i]
			if pidhash_entry['pid'] == pid:
				return cls(pidhash_entry['tcb'])
		return None

	@staticmethod
	def pids():
		"""return a list of all PIDs"""
		pidhash_sym = gdb.lookup_global_symbol('g_pidhash')
		pidhash_value = pidhash_sym.value()
		pidhash_type = pidhash_sym.type
		result = []
		for i in range(pidhash_type.range()[0],pidhash_type.range()[1]):
			entry = pidhash_value[i]
			pid = parse_int(entry['pid'])
			if pid != -1 and pid != 0xffff:
				result.append(pid)
		return result

	@staticmethod
	def tasks():
		"""return a list of all tasks"""
		tasks = []
		for pid in NX_task.pids():
			tasks.append(NX_task.for_pid(pid))
		return tasks

	def _state_is(self, state):
		"""tests the current state of the task against the passed-in state name"""
		statenames = gdb.types.make_enum_dict(gdb.lookup_type('enum tstate_e'))
		if self._tcb['task_state'] == statenames[state]:
			return True
		return False

	@property
	def stack_used(self):
		"""calculate the stack used by the thread"""
		stack_base = self._tcb['stack_alloc_ptr'].cast(gdb.lookup_type('unsigned char').pointer())
		if stack_base == 0:
			self.__dict__['stack_used'] = 0
		else:
			stack_limit = self._tcb['adj_stack_size']
			for offset in range(0, parse_int(stack_limit)):
				if stack_base[offset] != 0xff:
					break
			self.__dict__['stack_used'] = stack_limit - offset
		return self.__dict__['stack_used']

	@property
	def name(self):
		"""return the task's name"""
		return self._tcb['name'].string()

	@property
	def state(self):
		"""return the name of the task's current state"""
		statenames = gdb.types.make_enum_dict(gdb.lookup_type('enum tstate_e'))
		for name,value in statenames.items():
			if value == self._tcb['task_state']:
				return name
		return 'UNKNOWN'

	@property
	def waiting_for(self):
		"""return a description of what the task is waiting for, if it is waiting"""
		if self._state_is('TSTATE_WAIT_SEM'):
			try: 
				waitsem = self._tcb['waitsem'].dereference()

				# if 'holder' not in waitsem:
				# 	return 'no <holder>'

				waitsem_holder = waitsem['holder']
				holder = NX_task.for_tcb(waitsem_holder['htcb'])
				if holder is not None:
					return '{}({})'.format(waitsem.address, holder.name)
				else:
					return '{}(<bad holder>)'.format(waitsem.address)
			except:
				return 'EXCEPTION'
		if self._state_is('TSTATE_WAIT_SIG'):
			return 'signal'
		return ""

	@property
	def is_waiting(self):
		"""tests whether the task is waiting for something"""
		if self._state_is('TSTATE_WAIT_SEM') or self._state_is('TSTATE_WAIT_SIG'):
			return True

	@property
	def is_runnable(self):
		"""tests whether the task is runnable"""
		if (self._state_is('TSTATE_TASK_PENDING') or 
			self._state_is('TSTATE_TASK_READYTORUN') or 
			self._state_is('TSTATE_TASK_RUNNING')):
			return True
		return False

	@property
	def file_descriptors(self):
		"""return a dictionary of file descriptors and inode pointers"""
		filelist = self._group['tg_filelist']
		filearray = filelist['fl_files']
		result = dict()
		for i in range(filearray.type.range()[0],filearray.type.range()[1]):
			inode = parse_int(filearray[i]['f_inode'])
			if inode != 0:
				result[i] = inode
		return result

	@property
	def registers(self):
		if 'registers' not in self.__dict__:
			registers = dict()
			if self._state_is('TSTATE_TASK_RUNNING'):
				registers = NX_register_set.for_current().registers
			else:
				context = self._tcb['xcp']
				regs = context['regs']
				registers = NX_register_set.with_xcpt_regs(regs).registers

			self.__dict__['registers'] = registers
		return self.__dict__['registers']

	def __repr__(self):
		return "<NX_task {}>".format(self.pid)

	def __str__(self):
		return "{}:{}".format(self.pid, self.name)
	
	def showoff(self):
		print("-------")
		print("PID:\t",self.pid)
		print("Name:\t",self.name)
		print("State:\t",self.state)
		print("Waiting for:\t",self.waiting_for)
		print("Stack:\t",self.stack_used)
		print("Stack size:",self._tcb['adj_stack_size'])
		# print(self.file_descriptors)
		# print(self.registers)

	def __format__(self, format_spec):
		return format_spec.format(
                        address         =  self._tcb.address,
			pid              = self.pid,
			name             = self.name,
			state            = self.state,
			waiting_for      = self.waiting_for,
			stack_used       = self.stack_used,
			stack_limit      = self._tcb['adj_stack_size'],
			file_descriptors = self.file_descriptors,
			registers	 = self.registers
			)
###

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

class NuttxBacktrace(gdb.Command):
    def __init__(self):
        super(NuttxBacktrace, self).__init__("nx_bt", gdb.COMMAND_STACK)

    def invoke(self, arg, from_tty):
        self.task = NX_task.for_pid(parse_int(arg))
        # if self.task is not None:
        #     my_fmt = 'PID:{pid}  name:{name}  state:{state}\n'
        #     my_fmt += '  stack used {stack_used} of {stack_limit}\n'
        #     if self.task.is_waiting:
        #         my_fmt += '  waiting for {waiting_for}\n'
        #         my_fmt += '  open files: {file_descriptors}\n'
        #         my_fmt += '  R0  {registers[R0]:#010x} {registers[R1]:#010x} {registers[R2]:#010x} {registers[R3]:#010x}\n'
        #         my_fmt += '  R4  {registers[R4]:#010x} {registers[R5]:#010x} {registers[R6]:#010x} {registers[R7]:#010x}\n'
        #         my_fmt += '  R8  {registers[R8]:#010x} {registers[R9]:#010x} {registers[R10]:#010x} {registers[R11]:#010x}\n'
        #         my_fmt += '  R12 {registers[PC]:#010x}\n'
        #         my_fmt += '  SP  {registers[SP]:#010x} LR {registers[LR]:#010x} PC {registers[PC]:#010x} XPSR {registers[XPSR]:#010x}\n'
        #     print(format(self.task, my_fmt))
        #     print '-------------------'
        try:
            # Save a copy of the current CPU context.
            self.ctx = ARMRegContext(gdb.newest_frame())
            self.ctx.save_registers()

            frame = gdb.selected_frame()

            i = 0
            while frame != None:
                #if frame.pc() == 0:
                #    break

                annotations = self.annotate_frame(frame)

                if frame.name() == "interp_exec_method_full":
                    self.print_managed_frame(frame, i, annotations)
                else:
                    self.print_frame(frame, i, annotations)

                self.handle_frame(frame)
                i = i + 1

                # This can happen when we switch CPU context.
                if not frame.is_valid():
                    frame = gdb.selected_frame()
                else:
                    frame = frame.older()
        finally:
            self.ctx.restore_registers()

    def get_managed_frame_name(self, frame):
        frame_var_addr = int(frame.read_var("frame"))
        method_expr = "((struct _InterpFrame*) 0x%s).imethod->method" % format_hex(frame_var_addr)
        managed = gdb.parse_and_eval("mono_method_full_name(%s, 1)" % method_expr)
        managed = str(managed).translate(None, '"')
        return managed

    def print_managed_frame(self, frame, i, annotations):
        sal = frame.find_sal()
        print("#%s %s [%s]" % (i,
            self.get_managed_frame_name(frame), annotations))

    def print_frame(self, frame, i, annotations):
        annotations_text = ""
        if annotations != None:
            annotations_text = "[%s]" % (annotations)

        sal = frame.find_sal()
        symtab = sal.symtab
        filename = symtab.filename if symtab else ""

        print("#%s 0x%s %s () at %s:%s %s" % (i, format_hex(sal.pc),
            frame.name(), filename, sal.line, annotations_text))

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
            print "annnotating exception"
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
        nsyscalls = self.task._tcb['xcp']['nsyscalls']

        CONFIG_SYS_NNEST = 2
        assert nsyscalls <= CONFIG_SYS_NNEST

        index = nsyscalls - 1
        sysreturn = self.task._tcb['xcp']['syscall'][index]['sysreturn']

        pc = sysreturn
        print str(pc) + "<----"
        #print self.task._tcb['xcp']['regs']

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

        gdb.execute("set $sp = 0x%s" % format_hex(sp))
        gdb.execute("set $lr = 0x%s" % lr)
        gdb.execute("set $pc = 0x%s" % format_hex(pc))

        gdb.parse_and_eval("$sp = 0x%s" % format_hex(sp))
        print lr + "<----"
        gdb.parse_and_eval("$lr = 0x%s" % lr)
        gdb.parse_and_eval("$pc = 0x%s" % format_hex(pc))
        print str(sp) + "<----"
        gdb.execute("frame view 0x%s 0x%s" % (format_hex(sp), format_hex(pc)))
        gdb.execute("info frame")

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

            #print("set $sp = %s" % user_sp)
            #print("set $lr = 0x%s" % user_lr)
            #print("set $pc = 0x%s" % user_pc)

            gdb.parse_and_eval("$sp = %s" % (user_sp))
            gdb.parse_and_eval("$lr = 0x%s" % (user_lr))
            gdb.parse_and_eval("$pc = 0x%s" % (user_pc))

global_saved_regs_ctx = None

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

from gdb.unwinder import Unwinder

class FrameId(object):
    __slots__ = ['sp', 'pc']
    def __init__(self, sp, pc, special):
        self.sp = sp
        self.pc = pc
        #print type(pc)
        #self.special = special
        #print self.sp, self.pc


class NuttxUnwinder(Unwinder):
    def __init__(self):
        super(NuttxUnwinder, self).__init__("Nuttx kernel/user unwinder")
        self.flag = False

    def __call__(self, pending_frame):
        if self.flag == True:
            self.flag == False

        tcb_addr = gdb.execute("thread", to_string= True).split()[5][:-2]
        addr_value = gdb.Value(long(tcb_addr))
        tcb_ptr = addr_value.cast(gdb.lookup_type('struct tcb_s').pointer())
        tcb = tcb_ptr.dereference()

        nsyscalls = tcb['xcp']['nsyscalls']
        CONFIG_SYS_NNEST = 2
        assert nsyscalls <= CONFIG_SYS_NNEST

        index = nsyscalls - 1
        sysreturn = tcb['xcp']['syscall'][index]['sysreturn']

        sp = pending_frame.read_register("sp")
        pc = pending_frame.read_register("pc")
        print 'analyzing @', pc, sp

        if sysreturn == pending_frame.read_register("pc"):
            print "SWITCH FRAME"
            sp = pending_frame.read_register("sp")
            new_sp = read_memory_word(long(sp) + 20)
            print "new_sp = ", new_sp
            new_lr = read_memory_word(long(sp) + 32)
            print "new_lr = ", new_lr
            pc = pending_frame.read_register("pc")
            #fp = pending_frame.read_register("fp")
            lr = pending_frame.read_register("lr")
            print "sp = ", sp
            print "pc = ", pc
            #print "fp = ", fp
            print "lr = ", lr
            unwind_info = pending_frame.create_unwind_info(FrameId(sp, pc, lr))

            unwind_info.add_saved_register("sp", gdb.parse_and_eval("0x%s" % new_sp))
            unwind_info.add_saved_register("pc", lr)
            unwind_info.add_saved_register("lr", lr)
            return unwind_info

        #print gdb.parse_and_eval("dispatch_syscall + 100")
        if pc > gdb.parse_and_eval("dispatch_syscall + 100") or pc < gdb.parse_and_eval("dispatch_syscall"):
            print "Nothing to do"
            print "sp = ", pending_frame.read_register("sp")
            print "pc = ", pending_frame.read_register("pc")
            print "lr = ", pending_frame.read_register("lr")

            return None
        # Create UnwindInfo.  Usually the frame is identified by the stack 
        # pointer and the program counter.
 
        fp = read_memory_word(long(sp) + 36)
        lr = read_memory_word(long(sp) + 12)
        print "sp = ", sp
        print "pc = ", pc
        print "fp = ", fp
        print "lr = ", lr
        print "sysreturn = ", sysreturn
        unwind_info = pending_frame.create_unwind_info(FrameId(sp, pc, lr))

        # Find the values of the registers in the caller's frame and 
        # save them in the result:
        unwind_info.add_saved_register("sp", sp + 16) #gdb.parse_and_eval("0x%s" % fp) + 12)
        unwind_info.add_saved_register("pc", sysreturn)
        #unwind_info.add_saved_register("r11", gdb.parse_and_eval("0x%s" % fp))
        unwind_info.add_saved_register("lr", gdb.parse_and_eval("0x%s" % lr))

        print 'Created a custom frame.'
        self.flag = True
        return unwind_info

gdb.unwinder.register_unwinder(None, NuttxUnwinder(), replace = True)