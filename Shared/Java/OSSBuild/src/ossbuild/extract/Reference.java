package ossbuild.extract;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import ossbuild.StringUtil;
import ossbuild.init.Loader;

/**
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public final class Reference {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final Reference[]
		None = null
	;

	public static final Reference[]
		Empty = new Reference[0]
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private String name;
	private String title;
	private Resources resources;
	private Map<String, Reference> references;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public Reference(final String Name) {
		init(Name, Name, Resources.Empty, Reference.Empty);
	}

	public Reference(final String Name, final Resources Resources) {
		init(Name, Name, Resources, Reference.Empty);
	}

	public Reference(final String Name, final Resources Resources, final Reference... References) {
		init(Name, Name, Resources, References);
	}

	public Reference(final String Name, final String Title, final Resources Resources) {
		init(Name, Title, Resources, Reference.Empty);
	}

	public Reference(final String Name, final String Title, final Resources Resources, final Reference... References) {
		init(Name, Title, Resources, References);
	}

	private void init(final String Name, final String Title, final Resources Resources, final Reference[] References) {
		if (StringUtil.isNullOrEmpty(Name))
			throw new NullPointerException("Name must not be empty");
		if (Resources == null)
			throw new NullPointerException("Resources must not be null");

		this.name = Name;
		this.title = Title;
		this.resources = Resources;

		if (References == null || References == Empty || References == None || References.length <= 0)
			this.references = null;
		else
			addReferences(References);
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Getters">
	public String getName() {
		return name;
	}

	public String getTitle() {
		return title;
	}

	public Resources getResources() {
		return resources;
	}

	public boolean isLoaded() {
		return resources.isLoaded();
	}

	public boolean isProcessed() {
		return resources.isProcessed();
	}

	public Reference[] getReferences() {
		if (references == null)
			return Empty;
		return references.values().toArray(new Reference[references.size()]);
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Helper Methods">
	private Resources resourceForRef(final String referenceName) {
		if (StringUtil.isNullOrEmpty(referenceName))
			throw new MissingResourceReferenceException("An empty reference name is invalid.");
		final Reference ref = findReference(referenceName, true);
		if (ref == null)
			return null;
			//throw new MissingResourceReferenceException("Unable to find " + referenceName + " in the package registry.");
		return ref.getResources();
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Public Methods">
	public void visit(final Registry.IVisitor visitor, final boolean Recursive) {
		if (visitor == null || references == null || references.isEmpty())
			return;
		for(Reference r : references.values()) {
			visitor.visit(r);
			if (Recursive)
				r.visit(visitor, Recursive);
		}
	}

	public boolean isEmpty() {
		return references == null || references.isEmpty();
	}

	public int size() {
		return (references != null ? references.size() : 0);
	}

	public int getSize() {
		return size();
	}

	public boolean addReference(final Reference Reference) {
		return addReferences(Reference);
	}

	public boolean addReferences(final Reference...References) {
		if (References == null || References.length <= 0)
			return true;

		if (references == null)
			references = new HashMap<String, Reference>(5, 0.5f);

		for(Reference r : References)
			if (r != null)
				references.put(r.getName(), r);

		return true;
	}

	public boolean removeReference(final Reference Reference) {
		if (Reference == null)
			throw new NullPointerException("Reference must not be null");

		return removeReference(Reference.getName());
	}

	public boolean removeReference(final String Name) {
		if (StringUtil.isNullOrEmpty(Name))
			throw new NullPointerException("Name must not be empty");

		if (references == null || references.isEmpty() || !references.containsKey(Name))
			return true;

		references.remove(Name);
		return true;
	}

	public boolean clearReferences() {
		if (references == null || references.isEmpty())
			return true;
		references.clear();
		return true;
	}

	public boolean references(final String Name) {
		return (findReference(Name) != null);
	}

	public boolean references(final String Name, final boolean Recursive) {
		return (findReference(Name, Recursive) != null);
	}

	public Reference findReference(final String Name) {
		return findReference(Name, false);
	}

	public Reference findReference(final String Name, final boolean Recursive) {
		if (StringUtil.isNullOrEmpty(Name))
			return null;

		if (name.equalsIgnoreCase(Name))
			return this;

		if (references == null || references.isEmpty())
			return null;

		if (references.containsKey(Name))
			return references.get(Name);

		if (Recursive) {
			Reference ref;
			for(Map.Entry<String, Reference> e : references.entrySet())
				if ((ref = e.getValue().findReference(Name, Recursive)) != null)
					return ref;
			return null;
		} else {
			return null;
		}
	}

	public Future loadResources(final String referenceName) {
		final Resources res = resourceForRef(referenceName);
		if (res == null)
			return Resources.CompletedFuture;
		return res.extract();
	}

	public Future loadResources(final String referenceName, final IResourceCallback callback) {
		final Resources res = resourceForRef(referenceName);
		if (res == null)
			return Resources.CompletedFuture;
		return res.extract(callback);
	}

	public Future loadResources(final String referenceName, final IResourceProgressListener progress, final IResourceCallback callback) {
		final Resources res = resourceForRef(referenceName);
		if (res == null)
			return Resources.CompletedFuture;
		return res.extract(progress, callback);
	}

	public Future loadResources(final String referenceName, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		final Resources res = resourceForRef(referenceName);
		if (res == null)
			return Resources.CompletedFuture;
		return res.extract(filter, progress, callback);
	}

	public Future loadResources(final String referenceName, final ExecutorService executor, final IResourceFilter filter, final IResourceProgressListener progress, final IResourceCallback callback) {
		final Resources res = resourceForRef(referenceName);
		if (res == null)
			return Resources.CompletedFuture;
		return res.extract(executor, filter, progress, callback);
	}
	//</editor-fold>
}
