
package ossbuild.init;

import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathConstants;
import javax.xml.xpath.XPathException;
import javax.xml.xpath.XPathFactory;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;
import ossbuild.Namespaces;
import ossbuild.StringUtil;
import ossbuild.extract.Registry;
import ossbuild.extract.ResourceException;
import ossbuild.extract.Resources;

/**
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public class Loader {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  INIT_RESOURCE = "/resources/init.xml"
	;

	public static final String
		  ATTRIBUTE_CLASS = "class"
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private static boolean read = false;
	private static boolean systemLoadersUnloaded = true;
	private static boolean systemLoadersInitialized = false;
	private static boolean registryReferencesUnloaded = true;
	private static boolean registryReferencesInitialized = false;
	private static final Object lock = new Object();
	private static final Object systemLoaderLock = new Object();
	private static final Object registryReferenceLock = new Object();
	private static final List<SystemLoaderInfo> systemLoaders = new ArrayList<SystemLoaderInfo>(2);
	private static final List<RegistryReferenceInfo> registryReferences = new ArrayList<RegistryReferenceInfo>(5);
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Getters">
	public static Object getLock() {
		return lock;
	}

	public static Object getSystemLoaderLock() {
		return systemLoaderLock;
	}

	public static Object getRegistryReferenceLock() {
		return registryReferenceLock;
	}

	public static boolean isRead() {
		synchronized(lock) {
			return read;
		}
	}

	public static boolean areSystemLoadersInitialized() {
		synchronized(systemLoaderLock) {
			return systemLoadersInitialized;
		}
	}

	public static boolean areSystemLoadersUnloaded() {
		synchronized(systemLoaderLock) {
			return systemLoadersUnloaded;
		}
	}

	public static boolean areRegistryReferencesInitialized() {
		synchronized(lock) {
			return registryReferencesInitialized;
		}
	}

	public static boolean areRegistryReferencesUnloaded() {
		synchronized(lock) {
			return registryReferencesUnloaded;
		}
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Helper Methods">
	protected static String valueForAttribute(Node node, String name) {
		final Node attrib = node.getAttributes().getNamedItem(name);
		if (attrib == null || attrib.getNodeValue() == null)
			return StringUtil.empty;
		return attrib.getNodeValue();
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Public Static Methods">
	public static boolean initialize() throws Throwable {
		return initialize(true, null);
	}

	public static boolean initialize(final IInitializeListener Listener) throws Throwable {
		return initialize(true, Listener);
	}

	public static boolean initialize(final boolean cleanRegistryAfterInitialization) throws Throwable {
		return initialize(cleanRegistryAfterInitialization, null);
	}
	
	public static boolean initialize(final boolean cleanRegistryAfterInitialization, final IInitializeListener Listener) throws Throwable {
		synchronized(lock) {
			synchronized(systemLoaderLock) {
				synchronized(registryReferenceLock) {
					final boolean ret = initializeRegistryReferences(Listener) && initializeSystemLoaders(Listener);
					if (ret && cleanRegistryAfterInitialization)
						Registry.clear();
					return ret;
				}
			}
		}
	}
	
	public static boolean unload() throws Throwable {
		synchronized(lock) {
			synchronized(systemLoaderLock) {
				synchronized(registryReferenceLock) {
					return unloadSystemLoaders() && unloadRegistryReferences();
				}
			}
		}
	}

	//<editor-fold defaultstate="collapsed" desc="System Loaders">
	public static boolean initializeSystemLoaders() throws Throwable {
		return initializeSystemLoaders(ISystemLoaderInitializeListener.None);
	}

	public static boolean initializeSystemLoaders(final ISystemLoaderInitializeListener Listener) throws Throwable {
		synchronized(lock) {
			synchronized(systemLoaderLock) {
				if (systemLoadersInitialized) {
					//<editor-fold defaultstate="collapsed" desc="Call listener methods">
					if (Listener != null) {
						if (Listener.beforeAllSystemLoadersInitialized()) {
							if (!systemLoaders.isEmpty()) {
								for(SystemLoaderInfo info : systemLoaders) {
									if (Listener.beforeSystemLoaderInitialized(info.getInstance()))
										Listener.afterSystemLoaderInitialized(info.getInstance());
								}
							}
							Listener.afterAllSystemLoadersInitialized();
						}
					}
					//</editor-fold>
					return true;
				}

				if (!registryReferencesInitialized)
					throw new IllegalStateException("initializeRegistryReferences() must be called before initializeSystemLoaders()");

				systemLoadersInitialized = true;
				systemLoadersUnloaded = false;

				//Ensure that we unload when the application ends
				Runtime.getRuntime().addShutdownHook(new Thread() {
					@Override
					public void run() {
						Loader.unloadSystemLoaders();
					}
				});

				if (!read)
					read();

				if (Listener != null)
					Listener.beforeAllSystemLoadersInitialized();

				//If we have a loader, then call its .load() method
				if (!systemLoaders.isEmpty()) {
					for(SystemLoaderInfo info : systemLoaders) {
						if (info != null) {

							if (Listener != null && !Listener.beforeSystemLoaderInitialized(info.getInstance()))
								continue;

							info.load();
							
							if (Listener != null)
								Listener.afterSystemLoaderInitialized(info.getInstance());
						}
					}
				}

				if (Listener != null)
					Listener.afterAllSystemLoadersInitialized();

				return true;
			}
		}
	}

	public static boolean unloadSystemLoaders() {
		synchronized(lock) {
			synchronized(systemLoaderLock) {
				if (!systemLoadersInitialized)
					return true;
				if (systemLoadersUnloaded)
					return true;

				systemLoadersUnloaded = true;
				systemLoadersInitialized = false;

				try {
					for(SystemLoaderInfo info : systemLoaders)
						if (info != null)
							info.unload();
				} catch(Throwable t) {
					throw new RuntimeException(t);
				}
				return true;
			}
		}
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Registry References">
	public static boolean initializeRegistryReferences() throws Throwable {
		return initializeRegistryReferences(IRegistryReferenceInitializeListener.None);
	}
	
	public static boolean initializeRegistryReferences(final IRegistryReferenceInitializeListener Listener) throws Throwable {
		synchronized(lock) {
			synchronized(registryReferenceLock) {
				if (registryReferencesInitialized) {
					//<editor-fold defaultstate="collapsed" desc="Call listener methods">
					if (Listener != null) {
						if (Listener.beforeAllRegistryReferencesInitialized()) {
							if (!registryReferences.isEmpty()) {
								for(RegistryReferenceInfo info : registryReferences) {
									if (Listener.beforeRegistryReferenceInitialized(info.getInstance()))
										Listener.afterRegistryReferenceInitialized(info.getInstance());
								}
							}
							Listener.afterAllRegistryReferencesInitialized();
						}
					}
					//</editor-fold>
					return true;
				}

				registryReferencesInitialized = true;
				registryReferencesUnloaded = false;

				//Ensure that we unregister when the application ends
				Runtime.getRuntime().addShutdownHook(new Thread() {
					@Override
					public void run() {
						Loader.unloadRegistryReferences();
					}
				});

				if (!read)
					read();

				if (Listener != null)
					Listener.beforeAllRegistryReferencesInitialized();

				//If we have a registery ref, then call its .register() method
				if (!registryReferences.isEmpty()) {
					for(RegistryReferenceInfo info : registryReferences) {
						if (info != null) {
							
							if (Listener != null && !Listener.beforeRegistryReferenceInitialized(info.getInstance()))
								continue;

							info.register();
							
							final Resources res = info.createResourceExtractor();
							if (res != null && res != Resources.Empty)
								Registry.add(res.getName(), res);

							if (Listener != null)
								Listener.afterRegistryReferenceInitialized(info.getInstance());
						}
					}
				}

				if (Listener != null)
					Listener.afterAllRegistryReferencesInitialized();

				return true;
			}
		}
	}

	public static boolean unloadRegistryReferences() {
		synchronized(lock) {
			synchronized(registryReferenceLock) {
				if (!registryReferencesInitialized)
					return true;
				if (registryReferencesUnloaded)
					return true;

				registryReferencesUnloaded = true;
				registryReferencesInitialized = false;

				try {
					for(RegistryReferenceInfo info : registryReferences) {
						if (info != null) {
							info.unregister();
						}
					}
				} catch(Throwable t) {
					throw new RuntimeException(t);
				}
				return true;
			}
		}
	}
	//</editor-fold>
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="The Meat">
	public static boolean read() throws Throwable {
		synchronized(lock) {
			synchronized(systemLoaderLock) {
				synchronized(registryReferenceLock) {
					if (read)
						return true;

					read = true;

					final String resource = (INIT_RESOURCE.startsWith("/") ? INIT_RESOURCE.substring(1) : INIT_RESOURCE);

					try {
						final DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
						final DocumentBuilder builder = factory.newDocumentBuilder();
						final XPathFactory xpathFactory = XPathFactory.newInstance();
						final XPath xpath = xpathFactory.newXPath();

						//Creates a context that always has "ossbuild" as a prefix -- useful for xpath evaluations
						xpath.setNamespaceContext(Namespaces.createNamespaceContext());

						//Cycle through every file at /resources/init.xml. There can be multiple ones in different
						//jars - using the class loader's getResources() allows us to access all of them.
						URL url;
						Enumeration<URL> e = Thread.currentThread().getContextClassLoader().getResources(resource);
						while(e.hasMoreElements()) {
							url = e.nextElement();

							Document document = null;
							SystemLoaderInfo sysLoaderInfo = null;
							RegistryReferenceInfo regRefInfo = null;
							InputStream input = null;
							try {
								input = url.openStream();
								document = builder.parse(input);
							} catch (SAXException ex) {
								return false;
							} catch(IOException ie) {
								return false;
							} finally {
								try {
									if (input != null)
										input.close();
								} catch(IOException ie) {
								}
							}

							//Collapse whitespace nodes
							document.normalize();

							//Get the top-level document element, <System />
							final Element top = document.getDocumentElement();

							try {
								Node node;
								NodeList lst;

								//<editor-fold defaultstate="collapsed" desc="System loaders">
								//Locate <Load /> tags
								if ((lst = (NodeList)xpath.evaluate("//Init/System/Load", top, XPathConstants.NODESET)) != null && lst.getLength() > 0) {
									//Iterate over every <Load /> tag
									for(int i = 0; i < lst.getLength() && (node = lst.item(i)) != null; ++i) {

										//Examine the individual loader
										if ((sysLoaderInfo = readSystemLoader(xpath, document, node)) != null)
											systemLoaders.add(sysLoaderInfo);
									}
								}
								//</editor-fold>

								//<editor-fold defaultstate="collapsed" desc="Registry references">
								//Locate <Load /> tags
								if ((lst = (NodeList)xpath.evaluate("//Init/Registry/Reference", top, XPathConstants.NODESET)) != null && lst.getLength() > 0) {
									//Iterate over every <Reference /> tag
									for(int i = 0; i < lst.getLength() && (node = lst.item(i)) != null; ++i) {

										//Examine the individual registry reference
										if ((regRefInfo = readRegistryReference(xpath, document, node)) != null)
											registryReferences.add(regRefInfo);
									}
								}
								//</editor-fold>

							} catch(XPathException t) {
								return false;
							}
						}

						return true;
					} finally {
					}
				}
			}
		}
	}

	private static SystemLoaderInfo readSystemLoader(final XPath xpath, final Document document, final Node node) throws XPathException {
		final String className = valueForAttribute(node, ATTRIBUTE_CLASS).trim();
		if (StringUtil.isNullOrEmpty(className))
			return null;

		try {
			final Class cls = Class.forName(className, true, Thread.currentThread().getContextClassLoader());
			if (!ISystemLoader.class.isAssignableFrom(cls))
				return null;

			final ISystemLoader instance = (ISystemLoader)cls.newInstance();

			return new SystemLoaderInfo(cls, instance);
		} catch(Throwable t) {
			if (t instanceof ResourceException)
				throw (ResourceException)t;
			throw new ResourceException(t.getMessage(), t);
		}
	}

	private static RegistryReferenceInfo readRegistryReference(final XPath xpath, final Document document, final Node node) throws XPathException {
		final String className = valueForAttribute(node, ATTRIBUTE_CLASS).trim();
		if (StringUtil.isNullOrEmpty(className))
			return null;

		try {
			final Class cls = Class.forName(className, true, Thread.currentThread().getContextClassLoader());
			if (!IRegistryReference.class.isAssignableFrom(cls))
				return null;

			final IRegistryReference instance = (IRegistryReference)cls.newInstance();

			return new RegistryReferenceInfo(cls, instance);
		} catch(Throwable t) {
			if (t instanceof ResourceException)
				throw (ResourceException)t;
			throw new ResourceException(t.getMessage(), t);
		}
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Classes">
	private static class SystemLoaderInfo {
		//<editor-fold defaultstate="collapsed" desc="Variables">
		private Class loaderCls;
		private ISystemLoader instance;
		//</editor-fold>

		//<editor-fold defaultstate="collapsed" desc="Initialization">
		public SystemLoaderInfo(final Class cls, final ISystemLoader instance) {
			this.loaderCls = cls;
			this.instance = instance;
		}
		//</editor-fold>

		//<editor-fold defaultstate="collapsed" desc="Getters">
		public Class getSystemLoaderClass() {
			return loaderCls;
		}

		public ISystemLoader getInstance() {
			return instance;
		}
		//</editor-fold>

		//<editor-fold defaultstate="collapsed" desc="Public Methods">
		public void load() throws Throwable {
			if (instance != null)
				instance.load();
		}

		public void unload() throws Throwable {
			if (instance != null)
				instance.unload();
		}
		//</editor-fold>
	}

	private static class RegistryReferenceInfo {
		//<editor-fold defaultstate="collapsed" desc="Variables">
		private Class cls;
		private IRegistryReference instance;
		//</editor-fold>

		//<editor-fold defaultstate="collapsed" desc="Initialization">
		public RegistryReferenceInfo(final Class cls, final IRegistryReference instance) {
			this.cls = cls;
			this.instance = instance;
		}
		//</editor-fold>

		//<editor-fold defaultstate="collapsed" desc="Getters">
		public Class getRegistryReferenceClass() {
			return cls;
		}

		public IRegistryReference getInstance() {
			return instance;
		}
		//</editor-fold>

		//<editor-fold defaultstate="collapsed" desc="Public Methods">
		public void register() throws Throwable {
			if (instance != null)
				instance.register();
		}

		public void unregister() throws Throwable {
			if (instance != null)
				instance.unregister();
		}

		public Resources createResourceExtractor() throws Throwable {
			if (instance != null)
				return instance.createResourceExtractor();
			else
				return Resources.Empty;
		}
		//</editor-fold>
	}
	//</editor-fold>
}
