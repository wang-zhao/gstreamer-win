
package ossbuild.init;

/**
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public interface IRegistryReferenceInitializeListener {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final IRegistryReferenceInitializeListener 
		None = null
	;
	//</editor-fold>

	boolean beforeAllRegistryReferencesInitialized();
	void afterAllRegistryReferencesInitialized();

	boolean beforeRegistryReferenceInitialized(final IRegistryReference RegistryReference);
	void afterRegistryReferenceInitialized(final IRegistryReference RegistryReference);
}
