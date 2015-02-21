#include <ProcessRegistry.h>
#include <types.h>
#include "ArchSerialInfo.h"
#include "BDManager.h"
#include "BDVirtualDevice.h"
#include "PageManager.h"
#include "KernelMemoryManager.h"
#include "ArchInterrupts.h"
#include "ArchThreads.h"
#include "kprintf.h"
#include "Thread.h"
#include "Scheduler.h"
#include "ArchCommon.h"
#include "ArchThreads.h"
#include "Mutex.h"
#include "debug_bochs.h"
#include "ArchMemory.h"
#include "Loader.h"
#include "assert.h"
#include "SerialManager.h"
#include "KeyboardManager.h"
#include "VfsSyscall.h"
#include "FileSystemInfo.h"
#include "Dentry.h"
#include "DeviceFSType.h"
#include "VirtualFileSystem.h"
#include "TextConsole.h"
#include "FrameBufferConsole.h"
#include "Terminal.h"
#include "outerrstream.h"
#include "user_progs.h"

extern void* kernel_end_address;
extern Console* main_console;

uint8 boot_stack[0x4000] __attribute__((aligned(0x4000)));
uint32 boot_completed;
uint32 we_are_dying;
FileSystemInfo* default_working_dir;

extern "C" void initialiseBootTimePaging();
extern "C" void removeBootTimeIdentMapping();

extern "C" void startup()
{
  writeLine2Bochs("Removing Boot Time Ident Mapping...\n");
  removeBootTimeIdentMapping();
  we_are_dying = 0;
  boot_completed = 0;

  PageManager::instance();
  // Sets the reserved memory by the KernelMemoryManager (Kernel Heap)
  // If you erase the following two lines the memory will be reserved dynamically
  // please note that this means that the KMM depends on the page manager and you
  // will have a harder time implementing swapping. Pros only!
  KernelMemoryManager::instance()->setMinimumReservedMemory(1024 * 1024);
  KernelMemoryManager::instance()->setMaximumReservedMemory(1024 * 1024);
  writeLine2Bochs("PageManager and KernelMemoryManager created \n");

  //SerialManager::getInstance()->do_detection( 1 );

  main_console = ArchCommon::createConsole(1);
  writeLine2Bochs("Console created \n");

  Terminal *term_0 = main_console->getTerminal(0); // add more if you need more...

  term_0->setBackgroundColor(Console::BG_BLACK);
  term_0->setForegroundColor(Console::FG_GREEN);
  kprintfd("Init debug printf\n");
  term_0->writeString("This is on term 0, you should see me now\n");

  main_console->setActiveTerminal(0);

  kprintf("Kernel end address is %x\n", &kernel_end_address);

  Scheduler::instance();

  //needs to be done after scheduler and terminal, but prior to enableInterrupts
  kprintf_init();

  debug(MAIN, "Threads init\n");
  ArchThreads::initialise();
  debug(MAIN, "Interupts init\n");
  ArchInterrupts::initialise();

  ArchCommon::initDebug();

  vfs.initialize();
  debug(MAIN, "Mounting DeviceFS under /dev/\n");
  DeviceFSType *devfs = new DeviceFSType();
  vfs.registerFileSystem(devfs);
  default_working_dir = vfs.root_mount("devicefs", 0);

  debug(MAIN, "Block Device creation\n");
  BDManager::getInstance()->doDeviceDetection();
  debug(MAIN, "Block Device done\n");

  for (BDVirtualDevice* bdvd : BDManager::getInstance()->device_list_)
  {
    debug(MAIN, "Detected Device: %s :: %d\n", bdvd->getName(), bdvd->getDeviceNumber());
  }

  // initialise global and static objects
  extern ustl::list<FileDescriptor*> global_fd;
  new (&global_fd) ustl::list<FileDescriptor*>();
  extern Mutex global_fd_lock;
  new (&global_fd_lock) Mutex("global_fd_lock");

  debug(MAIN, "make a deep copy of FsWorkingDir\n");
  main_console->setWorkingDirInfo(new FileSystemInfo(*default_working_dir));
  debug(MAIN, "main_console->setWorkingDirInfo done\n");

  ustl::coutclass::init();
  debug(MAIN, "default_working_dir root name: %s\t pwd name: %s\n", default_working_dir->getRoot()->getName(),
        default_working_dir->getPwd()->getName());
  if (main_console->getWorkingDirInfo())
  {
    delete main_console->getWorkingDirInfo();
  }
  main_console->setWorkingDirInfo(default_working_dir);

  debug(MAIN, "Timer enable\n");
  ArchInterrupts::enableTimer();

  KeyboardManager::instance();
  ArchInterrupts::enableKBD();

  debug(MAIN, "Adding Kernel threads\n");

  Scheduler::instance()->addNewThread(main_console);

  Scheduler::instance()->addNewThread(new ProcessRegistry(new FileSystemInfo(*default_working_dir), user_progs) // see user_progs.h
                                                          );

  Scheduler::instance()->printThreadList();

  kprintf("Now enabling Interrupts...\n");
  boot_completed = 1;
  ArchInterrupts::enableInterrupts();

  Scheduler::instance()->yield();

  //not reached
  assert(false);
}
