import os
import ycm_core

flags = [
'-Wall',
'-Wextra',
'-Werror',
#'-DNDEBUG',
# Mips (Prevents assembly errors)
'-target',
'mips-harvard-os161-eabi',
# use gnu11 (c11)
'-std=gnu11',
# C only compilation
'-x',
'c',
# Try to remove all system includes and shared objects
'-nostdinc',
'-nostdlib',
'-nostdlibinc',
'-nolibc',
# ++ flags may not needed as we have -x c but better to be safe
'-nostdinc++',
'-nostdlib++',
]

mips_folder = 'kern/arch/'
# Kernel also matches all mips but thats fine since mips needs kernel includes anyways
kernel_folder = 'kern/'
kerndev_folder = 'kern/dev/'
user_folder = 'userland/'

def MipsAddFlags(flag_list):
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/mips')

def KernelAddFlags(flag_list):
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/kernel')
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/kernel2')
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/kernel3')
    flag_list.append('-I./.ycm_inc/kernel4')

def KernelDevAddFlags(flag_list):
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/kerndev')

def UserAddFlags(flag_list):
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/user')
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/user2')
    flag_list.append('-isystem')
    flag_list.append('.ycm_inc/user3')


def DirectoryOfThisScript():
  return os.path.dirname( os.path.abspath( __file__ ) )


def MakeRelativePathsInFlagsAbsolute( flags, working_directory ):
  if not working_directory:
    return list( flags )
  new_flags = []
  make_next_absolute = False
  path_flags = [ '-isystem', '-I', '-iquote', '--sysroot=' ]
  for flag in flags:
    new_flag = flag

    if make_next_absolute:
      make_next_absolute = False
      if not flag.startswith( '/' ):
        new_flag = os.path.join( working_directory, flag )

    for path_flag in path_flags:
      if flag == path_flag:
        make_next_absolute = True
        break

      if flag.startswith( path_flag ):
        path = flag[ len( path_flag ): ]
        new_flag = path_flag + os.path.join( working_directory, path )
        break

    if new_flag:
      new_flags.append( new_flag )
  return new_flags


def FlagsForFile( filename, **kwargs ):
    relative_to = DirectoryOfThisScript()
    final_flags = MakeRelativePathsInFlagsAbsolute( flags, relative_to )

    # Change includes based on the file location
    # A bit of a hack to make sure no incorrect files are included by accident
    if mips_folder in filename:
        MipsAddFlags(final_flags)

    if kernel_folder in filename:
        KernelAddFlags(final_flags)

    if user_folder in filename:
        UserAddFlags(final_flags)

    if kerndev_folder in filename:
        KernelDevAddFlags(final_flags)

    # Always add the files own folder
    final_flags.append('-I'+os.path.dirname(filename))

    final_flags = MakeRelativePathsInFlagsAbsolute(final_flags, relative_to)
    return { 'flags': final_flags }
