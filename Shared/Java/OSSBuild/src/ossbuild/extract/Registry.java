
package ossbuild.extract;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import ossbuild.init.Loader;

/**
 * Maintains list of resource collections and their references to other
 * resource collections.
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public final class Registry {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final Reference
		ROOT = new Reference(Registry.class.getName())
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	static {
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Getters">
	public static final Reference getRoot() {
		return ROOT;
	}

	public static final boolean isInitialized() {
		return Loader.areRegistryReferencesInitialized();
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Public Methods">
	public static void visit(final IVisitor visitor) {
		ROOT.visit(visitor, false);
	}

	public static void visit(final IVisitor visitor, final boolean Recursive) {
		ROOT.visit(visitor, Recursive);
	}

	public static final boolean isEmpty() {
		return ROOT.isEmpty();
	}

	public static final int size() {
		return ROOT.size();
	}

	public static final int getSize() {
		return ROOT.getSize();
	}

	public static final Reference add(final String Name, final Resources Resources) {
		final Reference ref = new Reference(Name, Resources);
		if (ROOT.addReference(ref))
			return ref;
		return null;
	}

	public static final Reference add(final String Name, final Resources Resources, final Reference... References) {
		final Reference ref = new Reference(Name, Resources, References);
		if (ROOT.addReference(ref))
			return ref;
		return null;
	}

	public static final boolean remove(final String Name) {
		return ROOT.removeReference(Name);
	}

	public static final boolean clear() {
		return ROOT.clearReferences();
	}

	public static final boolean hasReference(final String Name) {
		return ROOT.references(Name);
	}

	public static final boolean hasReference(final String Name, final boolean Recursive) {
		return ROOT.references(Name, Recursive);
	}

	public static final Reference findReference(final String Name) {
		return ROOT.findReference(Name);
	}

	public static final Reference findReference(final String Name, final boolean Recursive) {
		return ROOT.findReference(Name, Recursive);
	}

	public static final Future loadResources(final String referenceName) {
		return ROOT.loadResources(referenceName);
	}

	public static final Future loadResources(final String referenceName, final IResourceCallback callback) {
		return ROOT.loadResources(referenceName, callback);
	}

	public static final Future loadResources(final String referenceName, final IResourceProgressListener progress, final IResourceCallback callback) {
		return ROOT.loadResources(referenceName, progress, callback);
	}

	public static final Future loadResources(final String referenceName, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		return ROOT.loadResources(referenceName, filter, progress, callback);
	}

	public static final Future loadResources(final String referenceName, final ExecutorService executor, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		return ROOT.loadResources(referenceName, executor, filter, progress, callback);
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Interfaces">
	public static interface IVisitor {
		boolean visit(final Reference Reference);
	}
	//</editor-fold>
}
