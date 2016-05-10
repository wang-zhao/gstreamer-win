
package ossbuild.init;

/**
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public abstract class SystemLoaderInitializeListenerAdapter implements ISystemLoaderInitializeListener {
	@Override
	public boolean beforeAllSystemLoadersInitialized() {
		return true;
	}

	@Override
	public void afterAllSystemLoadersInitialized() {
	}

	@Override
	public boolean beforeSystemLoaderInitialized(final ISystemLoader SystemLoader) {
		return true;
	}

	@Override
	public void afterSystemLoaderInitialized(final ISystemLoader SystemLoader) {
	}
}
