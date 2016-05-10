
package ossbuild.init;

/**
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public interface ISystemLoaderInitializeListener {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final ISystemLoaderInitializeListener 
		None = null
	;
	//</editor-fold>

	boolean beforeAllSystemLoadersInitialized();
	void afterAllSystemLoadersInitialized();

	boolean beforeSystemLoaderInitialized(final ISystemLoader SystemLoader);
	void afterSystemLoaderInitialized(final ISystemLoader SystemLoader);
}
