
package ossbuild.init;

/**
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public abstract class InitializeListenerAdapter implements IInitializeListener {
	@Override
	public boolean beforeAllRegistryReferencesInitialized() {
		return true;
	}

	@Override
	public void afterAllRegistryReferencesInitialized() {
	}

	@Override
	public boolean beforeAllSystemLoadersInitialized() {
		return true;
	}

	@Override
	public void afterAllSystemLoadersInitialized() {
	}

	@Override
	public boolean beforeRegistryReferenceInitialized(final IRegistryReference RegistryReference) {
		return true;
	}

	@Override
	public void afterRegistryReferenceInitialized(final IRegistryReference RegistryReference) {
	}

	@Override
	public boolean beforeSystemLoaderInitialized(final ISystemLoader SystemLoader) {
		return true;
	}

	@Override
	public void afterSystemLoaderInitialized(final ISystemLoader SystemLoader) {
	}
}
