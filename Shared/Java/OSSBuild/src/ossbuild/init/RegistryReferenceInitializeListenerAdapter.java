
package ossbuild.init;

/**
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public abstract class RegistryReferenceInitializeListenerAdapter implements IRegistryReferenceInitializeListener {
	@Override
	public boolean beforeAllRegistryReferencesInitialized() {
		return true;
	}

	@Override
	public void afterAllRegistryReferencesInitialized() {
	}

	@Override
	public boolean beforeRegistryReferenceInitialized(final IRegistryReference RegistryReference) {
		return true;
	}

	@Override
	public void afterRegistryReferenceInitialized(final IRegistryReference RegistryReference) {
	}
}
