
package ossbuild.init;

import ossbuild.extract.Resources;

/**
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public interface IRegistryReference {
	void register() throws Throwable;
	void unregister() throws Throwable;
	Resources createResourceExtractor();
}
