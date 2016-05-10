
package ossbuild;

import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import ossbuild.extract.IResourceCallback;
import ossbuild.extract.IResourceFilter;
import ossbuild.extract.IResourceProgressListener;
import ossbuild.extract.Registry;
import ossbuild.init.IInitializeListener;
import ossbuild.init.IRegistryReferenceInitializeListener;
import ossbuild.init.ISystemLoaderInitializeListener;
import ossbuild.init.Loader;

/**
 * Utilities for accessing various system attributes and configuration.
 * 
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public final class Sys {
	//<editor-fold defaultstate="collapsed" desc="Getters">
	public static OS getOS() {
		return OS.getSystemOS();
	}

	public static OSFamily getOSFamily() {
		return OS.getSystemOSFamily();
	}

	public static Arch getArch() {
		return Arch.getSystemArch();
	}

	public static int getPID() {
		return Process.getPID();
	}

	public static int getLastError() {
		return Process.getLastError();
	}

	public static String getPath() {
		return Env.getPath();
	}

	public static String getEnvironmentVariable(final String name) {
		return Env.getEnvironmentVariable(name);
	}

	public static boolean isRegistryInitialized() {
		return Registry.isInitialized();
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Helper Methods">
	private static boolean loadResourcesAndWaitHelper(final Future f) {
		if (f == null)
			return false;
		try {
			Object o = f.get();
			return true;
		} catch(CancellationException t) {
			return false;
		} catch(ExecutionException t) {
			return false;
		} catch(InterruptedException t) {
			return false;
		}
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Public Static Methods">
	public static boolean isOS(final OS OS) {
		return (OS.getSystemOS() == OS);
	}

	public static boolean isOSFamily(final OSFamily OSFamily) {
		return (OS.getSystemOSFamily() == OSFamily);
	}

	public static boolean isArch(final Arch Arch) {
		return (Arch.getSystemArch() == Arch);
	}

	public static boolean isResourceAvailable(final String ResourceName) {
		final String res = (ResourceName.startsWith("/") && ResourceName.length() > 1 ? ResourceName.substring(1) : ResourceName).trim();
		if (StringUtil.isNullOrEmpty(res))
			return false;
		return (Thread.currentThread().getContextClassLoader().getResource(res) != null);
	}

	public static boolean setEnvironmentVariable(final String name, final String value) {
		return Env.setEnvironmentVariable(name, value);
	}

	public static boolean unsetEnvironmentVariable(final String name) {
		return Env.unsetEnvironmentVariable(name);
	}

	public static String createPlatformName() {
		return createPlatformName(getOSFamily());
	}

	public static String createPlatformName(OS OS) {
		return OS.getPlatformPartName() + "." + getArch().getPlatformPartName();
	}

	public static String createPlatformName(OSFamily OSFamily) {
		return OSFamily.getPlatformPartName() + "." + getArch().getPlatformPartName();
	}

	public static String createPackageResourcePrefix(final String PackagePrefix, final String PackageSuffix) {
		final String prefix = PackagePrefix.trim();
		final String suffix = PackageSuffix.trim();
		return
			'/' +
			(!StringUtil.isNullOrEmpty(prefix) ? prefix.replace('.', '/') + '/' : StringUtil.empty) +
			(!StringUtil.isNullOrEmpty(suffix) ? suffix.replace('.', '/') + '/' : StringUtil.empty)
		;
	}

	public static String createPackageResourcePrefix(final String PackagePrefix) {
		return createPackageResourcePrefix(PackagePrefix, StringUtil.empty);
	}

	public static String createPlatformPackageName(final String PackagePrefix, final String PlatformName, final String PackageSuffix) {
		final String prefix = PackagePrefix.trim();
		final String suffix = PackageSuffix.trim();

		String ret = PlatformName;
		
		if (!StringUtil.isNullOrEmpty(prefix)) {
			if (prefix.endsWith("."))
				ret = prefix + ret;
			else
				ret = prefix + "." + ret;
		}

		if (!StringUtil.isNullOrEmpty(suffix)) {
			if (suffix.startsWith("."))
				ret = ret + suffix;
			else
				ret = ret + "." + suffix;
		}

		return ret;
	}

	public static String createPlatformPackageName(final String PackagePrefix, final String PackageSuffix) {
		return createPlatformPackageName(PackagePrefix, createPlatformName(), PackageSuffix);
	}

	public static String createPlatformPackageName(final String PackagePrefix) {
		return createPlatformPackageName(PackagePrefix, createPlatformName(), StringUtil.empty);
	}

	public static String createPlatformPackageName(final String PackagePrefix, final OS OS, final String PackageSuffix) {
		return createPlatformPackageName(PackagePrefix, createPlatformName(OS), PackageSuffix);
	}

	public static String createPlatformPackageName(final String PackagePrefix, final OS OS) {
		return createPlatformPackageName(PackagePrefix, createPlatformName(OS), StringUtil.empty);
	}

	public static String createPlatformPackageName(final String PackagePrefix, final OSFamily OSFamily, final String PackageSuffix) {
		return createPlatformPackageName(PackagePrefix, createPlatformName(OSFamily), PackageSuffix);
	}

	public static String createPlatformPackageName(final String PackagePrefix, final OSFamily OSFamily) {
		return createPlatformPackageName(PackagePrefix, createPlatformName(OSFamily), StringUtil.empty);
	}

	public static String createPlatformPackageResourcePrefix(final String PackagePrefix, final String PackageSuffix) {
		return createPackageResourcePrefix(createPlatformPackageName(PackagePrefix, PackageSuffix));
	}

	public static String createPlatformPackageResourcePrefix(final String PackagePrefix) {
		return createPackageResourcePrefix(createPlatformPackageName(PackagePrefix));
	}

	public static String createPlatformPackageResourcePrefix(final String PackagePrefix, final OS OS, final String PackageSuffix) {
		return createPackageResourcePrefix(createPlatformPackageName(PackagePrefix, OS, PackageSuffix));
	}

	public static String createPlatformPackageResourcePrefix(final String PackagePrefix, final OS OS) {
		return createPackageResourcePrefix(createPlatformPackageName(PackagePrefix, OS));
	}

	public static String createPlatformPackageResourcePrefix(final String PackagePrefix, final OSFamily OSFamily, final String PackageSuffix) {
		return createPackageResourcePrefix(createPlatformPackageName(PackagePrefix, OSFamily, PackageSuffix));
	}

	public static String createPlatformPackageResourcePrefix(final String PackagePrefix, final OSFamily OSFamily) {
		return createPackageResourcePrefix(createPlatformPackageName(PackagePrefix, OSFamily));
	}

	public static String createPlatformPackageResourceName(final String PackagePrefix, final String PackageSuffix, final String ResourceName) {
		return createPlatformPackageResourcePrefix(PackagePrefix, PackageSuffix) + ResourceName;
	}

	public static String createPlatformPackageResourceName(final String PackagePrefix, final String ResourceName) {
		return createPlatformPackageResourcePrefix(PackagePrefix) + ResourceName;
	}

	public static String createPlatformPackageResourceName(final String PackagePrefix, final OS OS, final String PackageSuffix, final String ResourceName) {
		return createPlatformPackageResourcePrefix(PackagePrefix, OS, PackageSuffix) + ResourceName;
	}

	public static String createPlatformPackageResourceName(final String PackagePrefix, final OS OS, final String ResourceName) {
		return createPlatformPackageResourcePrefix(PackagePrefix, OS) + ResourceName;
	}

	public static String createPlatformPackageResourceName(final String PackagePrefix, final OSFamily OSFamily, final String PackageSuffix, final String ResourceName) {
		return createPlatformPackageResourcePrefix(PackagePrefix, OSFamily, PackageSuffix) + ResourceName;
	}

	public static String createPlatformPackageResourceName(final String PackagePrefix, final OSFamily OSFamily, final String ResourceName) {
		return createPlatformPackageResourcePrefix(PackagePrefix, OSFamily) + ResourceName;
	}

	public static boolean initializeRegistry() {
		try {
			return Loader.initializeRegistryReferences();
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean initializeRegistry(final IRegistryReferenceInitializeListener Listener) {
		try {
			return Loader.initializeRegistryReferences(Listener);
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean initializeSystem() {
		try {
			return Loader.initializeSystemLoaders();
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean initializeSystem(final ISystemLoaderInitializeListener Listener) {
		try {
			return Loader.initializeSystemLoaders(Listener);
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean initialize() {
		try {
			return Loader.initialize();
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean initialize(final IInitializeListener Listener) {
		try {
			return Loader.initialize(Listener);
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean initialize(final boolean cleanRegistryAfterInitialization) {
		try {
			return Loader.initialize(cleanRegistryAfterInitialization);
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean initialize(final boolean cleanRegistryAfterInitialization, final IInitializeListener Listener) {
		try {
			return Loader.initialize(cleanRegistryAfterInitialization, Listener);
		} catch(Throwable t) {
			throw new RuntimeException(t);
		}
	}

	public static boolean cleanRegistry() {
		return Registry.clear();
	}

	public static Future loadNativeResourcesAsync(final String registryReferenceName) {
		return Registry.loadResources(registryReferenceName);
	}

	public static Future loadNativeResourcesAsync(final String registryReferenceName, final IResourceCallback callback) {
		return Registry.loadResources(registryReferenceName, callback);
	}

	public static Future loadNativeResourcesAsync(final String registryReferenceName, final IResourceProgressListener progress, final IResourceCallback callback) {
		return Registry.loadResources(registryReferenceName, progress, callback);
	}

	public static Future loadNativeResourcesAsync(final String registryReferenceName, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		return Registry.loadResources(registryReferenceName, filter, progress, callback);
	}

	public static Future loadNativeResourcesAsync(final String registryReferenceName, final ExecutorService executor, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		return Registry.loadResources(registryReferenceName, executor, filter, progress, callback);
	}

	public static boolean loadNativeResources(final String registryReferenceName) {
		return loadResourcesAndWaitHelper(loadNativeResourcesAsync(registryReferenceName));
	}

	public static boolean loadNativeResources(final String registryReferenceName, final IResourceCallback callback) {
		return loadResourcesAndWaitHelper(loadNativeResourcesAsync(registryReferenceName, callback));
	}

	public static boolean loadNativeResources(final String registryReferenceName, final IResourceProgressListener progress, final IResourceCallback callback) {
		return loadResourcesAndWaitHelper(loadNativeResourcesAsync(registryReferenceName, progress, callback));
	}

	public static boolean loadNativeResources(final String registryReferenceName, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		return loadResourcesAndWaitHelper(loadNativeResourcesAsync(registryReferenceName, filter, progress, callback));
	}

	public static boolean loadNativeResources(final String registryReferenceName, final ExecutorService executor, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		return loadResourcesAndWaitHelper(loadNativeResourcesAsync(registryReferenceName, executor, filter, progress, callback));
	}
	//</editor-fold>
}
