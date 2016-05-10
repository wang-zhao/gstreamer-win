
package ossbuild.init;

import static org.junit.Assert.*;

/**
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public class TestSystemLoader extends SystemLoader {

	@Override
	public void load() throws Throwable {
		assertTrue(true);
	}

	@Override
	public void unload() throws Throwable {
		assertTrue(true);
	}

}
