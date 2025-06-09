import os
import sys
import time

# 尝试导入特定于操作系统的文件锁模块
try:
    import fcntl # Unix-like systems
except ImportError:
    fcntl = None

try:
    import msvcrt # Windows systems
except ImportError:
    msvcrt = None

class SingleInstanceLock:
    """
    一个跨平台的单实例锁类，使用文件锁定机制。
    """
    def __init__(self, lock_file_name="my_app_single_instance.lock"):
        """
        初始化单实例锁。
        :param lock_file_name: 用于锁定的文件名。建议使用应用程序的唯一标识符。
        """
        # 确保锁文件路径是可写的，且不会干扰其他文件
        # 使用用户临时目录或应用程序数据目录
        app_data_dir = self._get_app_data_dir()
        os.makedirs(app_data_dir, exist_ok=True) # 确保目录存在
        self.lock_file_path = os.path.join(app_data_dir, lock_file_name)
        self.lock_file_obj = None
        self.is_locked = False
        
        # 调试模式标志
        self.debug_mode = False # 默认关闭，可以在外部设置

    def _get_app_data_dir(self):
        """
        获取应用程序数据目录的路径。
        """
        if sys.platform == "win32":
            # Windows: %APPDATA%
            return os.path.join(os.environ.get('APPDATA', os.path.expanduser('~')), "AI_Screenshot_Translator")
        elif sys.platform == "darwin":
            # macOS: ~/Library/Application Support
            return os.path.join(os.path.expanduser('~'), 'Library', 'Application Support', "AI_Screenshot_Translator")
        else:
            # Linux/Unix: ~/.local/share 或 ~/.config
            return os.path.join(os.path.expanduser('~'), '.config', "AI_Screenshot_Translator")
            # 或者 os.path.join(os.path.expanduser('~'), '.local', 'share', "YourAppName")


    def acquire_lock(self):
        """
        尝试获取文件锁。
        :return: True 如果成功获取锁，False 如果锁已被其他实例持有。
        """
        if self.is_locked:
            if self.debug_mode:
                print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [SingleInstanceLock] 警告: 锁已获取，跳过重复获取尝试。")
            return True

        try:
            # 'a+b' 模式：以二进制追加模式打开，如果文件不存在则创建
            # 确保文件指针在文件开头，以便锁定整个文件
            self.lock_file_obj = open(self.lock_file_path, 'a+b')
            self.lock_file_obj.seek(0) # 将文件指针移到文件开头

            if sys.platform == "win32":
                if msvcrt is None:
                    raise RuntimeError("msvcrt 模块在 Windows 上不可用。")
                # msvcrt.locking(fd, mode, nbytes)
                # mode: msvcrt.LK_NBLCK (非阻塞独占锁)
                # nbytes: 1 (锁定一个字节，通常足够表示整个文件被锁定)
                msvcrt.locking(self.lock_file_obj.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                if fcntl is None:
                    raise RuntimeError("fcntl 模块在非 Windows 系统上不可用。")
                # fcntl.flock(fd, operation)
                # operation: fcntl.LOCK_EX | fcntl.LOCK_NB (独占锁，非阻塞)
                fcntl.flock(self.lock_file_obj.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)

            self.is_locked = True
            if self.debug_mode:
                print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [SingleInstanceLock] 成功获取文件锁: {self.lock_file_path}")
            return True
        except (IOError, OSError) as e:
            # 无法获取锁 (文件已被锁定或权限问题)
            if self.debug_mode:
                print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [SingleInstanceLock] 无法获取文件锁: {self.lock_file_path} - {e}")
            self.is_locked = False
            # 关闭文件对象，即使获取锁失败也要关闭
            if self.lock_file_obj:
                self.lock_file_obj.close()
                self.lock_file_obj = None
            return False
        except Exception as e:
            if self.debug_mode:
                print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [SingleInstanceLock] 获取锁时发生意外错误: {e}")
            self.is_locked = False
            if self.lock_file_obj:
                self.lock_file_obj.close()
                self.lock_file_obj = None
            return False

    def release_lock(self):
        """
        释放文件锁。
        """
        if self.is_locked and self.lock_file_obj:
            try:
                if sys.platform == "win32":
                    if msvcrt:
                        # msvcrt.LK_UNLCK (解锁)
                        msvcrt.locking(self.lock_file_obj.fileno(), msvcrt.LK_UNLCK, 1)
                else:
                    if fcntl:
                        # fcntl.LOCK_UN (解锁)
                        fcntl.flock(self.lock_file_obj.fileno(), fcntl.LOCK_UN)
                
                self.lock_file_obj.close()
                self.lock_file_obj = None
                self.is_locked = False
                if self.debug_mode:
                    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [SingleInstanceLock] 成功释放文件锁: {self.lock_file_path}")

            except Exception as e:
                if self.debug_mode:
                    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [SingleInstanceLock] 释放锁时发生错误: {e}")
        elif self.debug_mode:
            print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] [SingleInstanceLock] 警告: 尝试释放未获取的锁或无效的文件对象。")

    def __enter__(self):
        """上下文管理器入口"""
        if not self.acquire_lock():
            raise RuntimeError("无法获取应用程序单实例锁，可能已有实例在运行。")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """上下文管理器出口"""
        self.release_lock()

if __name__ == "__main__":
    lock = SingleInstanceLock("AI_Screenshot_Translator_Test.lock")
    lock.debug_mode = True # 开启调试模式

    if lock.acquire_lock():
        print("当前是第一个实例，应用程序正在运行...")
        # 模拟应用程序运行
        time.sleep(5)
        print("应用程序运行结束。")
        lock.release_lock()
    else:
        print("应用程序已在运行，退出当前实例。")

    # 再次尝试获取锁
    print("\n再次尝试获取锁...")
    if lock.acquire_lock():
        print("再次成功获取锁。")
        lock.release_lock()
    else:
        print("再次获取锁失败。")