
package ossbuild;

import com.sun.jna.Library;
import com.sun.jna.Native;
import java.io.File;
import ossbuild.extract.ResourceUtils;
import ossbuild.platform.Win32Library;

/**
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public class Path {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  directorySeparator = System.getProperty("file.separator")
		, pathSeparator = File.pathSeparator
	;

	public static final String
		  tempDirectory = clean(System.getProperty("java.io.tmpdir"))
		, homeDirectory = clean(System.getProperty("user.home"))
		, workingDirectory = clean(new File(".").getAbsolutePath())
	;
	
	public static final String 
		  nativeResourcesDirectoryName = "native_java_resources"
	;

	public static final String
		  nativeResourcesDirectory
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="JNA Library Declarations">
	interface PathLibraryWindows extends Win32Library {
		//<editor-fold defaultstate="collapsed" desc="Constants">
		public static final String
			LIB_NAME = ""
		;

		public static final PathLibraryWindows
			INSTANCE = null;//(PathLibraryWindows)Native.loadLibrary(LIB_NAME, PathLibraryWindows.class, DEFAULT_OPTIONS)
		;
		//</editor-fold>
	}

	interface PathLibraryUnix extends Library {
		//<editor-fold defaultstate="collapsed" desc="Constants">
		public static final String
			LIB_NAME = "c"
		;

		public static final PathLibraryUnix
			INSTANCE = (PathLibraryUnix)Native.loadLibrary(LIB_NAME, PathLibraryUnix.class);
		;
		//</editor-fold>

		public int symlink(final String to, final String from);
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private static Library nativelib;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	static {
		nativeResourcesDirectory = Path.combine(tempDirectory, nativeResourcesDirectoryName).getAbsolutePath();

		switch(OS.getSystemOSFamily()) {
			case Unix:
			case Mac:
				nativelib = PathLibraryUnix.INSTANCE;
				break;
			case Windows:
				nativelib = PathLibraryWindows.INSTANCE;
				break;
			default:
				break;
		}
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Public Static Methods">
	public static String nativeDirectorySeparator(final String path) {
		if (path == null)
			return StringUtil.empty;
		return path.replace("/", directorySeparator).replace("\\", directorySeparator);
	}

	public static String clean(final String path) {
		if (StringUtil.isNullOrEmpty(path))
			return StringUtil.empty;

		if (path.endsWith(directorySeparator))
			return path;

		if (path.endsWith("/") || path.endsWith("\\"))
			return path.substring(0, path.length() - 1) + directorySeparator;

		return path + directorySeparator;
	}

	public static File combine(final File parent, final String child) {
		return new File(parent, child);
	}

	public static File combine(final String parent, final String child) {
		return new File(parent, child);
	}

	public static boolean exists(final String path) {
		return exists(new File(path));
	}

	public static boolean exists(final File path) {
		if (path == null)
			return false;
		return path.exists();
	}

	public static boolean delete(final String path) {
		return delete(new File(path));
	}

	public static boolean delete(final File path) {
		if (path == null)
			return false;
		
		try {
			//True b/c the intent of this function is satisfied -- the directory/file no longer exists!
			if (!path.exists())
				return true;

			if (path.isFile())
				return path.delete();
			else
				return ResourceUtils.deleteDirectory(path);
		} catch(SecurityException se) {
			return false;
		}
	}

	public static boolean createSymbolicLink(final File to, final File from) {
		if (from == null || to == null)
			return false;
		return createSymbolicLink(to.getAbsolutePath(), from.getAbsolutePath());
	}
	
	public static boolean createSymbolicLink(final String to, final String from) {
		if (StringUtil.isNullOrEmpty(from) || StringUtil.isNullOrEmpty(to))
			throw new IllegalArgumentException("from and to cannot be empty");

		if (nativelib == null)
			throw new UnsupportedOperationException("Creating symbolic links is unsupported on this platform");

		if (nativelib instanceof PathLibraryUnix)
			return (PathLibraryUnix.INSTANCE.symlink(to, from) == 0);
		else if (nativelib instanceof PathLibraryWindows)
			return false;
		else
			throw new UnsatisfiedLinkError("Platform specific library for path manipulation is unavailable");
	}
	//</editor-fold>
}
