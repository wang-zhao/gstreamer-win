
package ossbuild.extract;

/**
 * Implements {@link IResourceCallback} and provides genericized parameters
 * for convencience.
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public abstract class ResourceCallback<T> implements IResourceCallback {
	//<editor-fold defaultstate="collapsed" desc="Variables">
	private T param;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public ResourceCallback() {
		init(null);
	}

	public ResourceCallback(final T Parameter) {
		init(Parameter);
	}

	private void init(final T Parameter) {
		this.param = Parameter;
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Getters">
	public T getParameter() {
		return param;
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="IResourceCallback Methods">
	@Override
	public void error(Resources resources) {
		error(resources, param);
	}

	@Override
	public void prepare(Resources resources) {
		prepare(resources, param);
	}

	@Override
	public void cancelled(Resources resources) {
		cancelled(resources, param);
	}

	@Override
	public void completed(Resources resources) {
		completed(resources, param);
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Overrides">
	protected void error(Resources resources, T param) {
	}

	protected void prepare(Resources resources, T param) {
	}

	protected void cancelled(Resources resources, T param) {
	}
	//</editor-fold>
	
	//<editor-fold defaultstate="collapsed" desc="Abstract Methods">
	protected abstract void completed(Resources resources, T param);
	//</editor-fold>
}
