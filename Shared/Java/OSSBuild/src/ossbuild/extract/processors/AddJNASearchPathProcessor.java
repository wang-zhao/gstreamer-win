
package ossbuild.extract.processors;

import com.sun.jna.NativeLibrary;
import java.io.File;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathException;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import ossbuild.StringUtil;
import ossbuild.extract.DefaultResourceProcessor;
import ossbuild.extract.IResourceFilter;
import ossbuild.extract.IResourcePackage;
import ossbuild.extract.IResourceProgressListener;
import ossbuild.extract.IVariableProcessor;
import ossbuild.extract.ResourceProcessor;
import ossbuild.extract.ResourceProcessorFactory;

/**
 * Changes the process' working directory (cwd).
 * 
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
@ResourceProcessor(
	tagName = "AddJNASearchPath",
	supportsSize = false
)
public class AddJNASearchPathProcessor extends DefaultResourceProcessor {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  ATTRIBUTE_PATH            = "path"
		, ATTRIBUTE_LIBRARY_NAME    = "libraryName"
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private String path = StringUtil.empty;
	private String libraryName = StringUtil.empty;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public AddJNASearchPathProcessor() {
	}

	public AddJNASearchPathProcessor(String libraryName, File path) {
		this(libraryName, path.getAbsolutePath(), StringUtil.empty, StringUtil.empty);
	}

	public AddJNASearchPathProcessor(String libraryName, String path) {
		this(libraryName, path, StringUtil.empty, StringUtil.empty);
	}

	public AddJNASearchPathProcessor(String libraryName, String path, String Title, String Description) {
		super(false, StringUtil.empty, StringUtil.empty, StringUtil.empty, Title, Description);

		this.path = path;
		this.libraryName = libraryName;
	}
	//</editor-fold>
	
	@Override
	protected boolean loadSettings(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		this.path = stringAttributeValue(varproc, StringUtil.empty, node, ATTRIBUTE_PATH);
		this.libraryName = stringAttributeValue(varproc, StringUtil.empty, node, ATTRIBUTE_LIBRARY_NAME);
		
		return true;
	}

	@Override
	protected boolean processResource(final String fullResourceName, final IResourcePackage pkg, final IResourceFilter filter, final IResourceProgressListener progress) {
		if (StringUtil.isNullOrEmpty(libraryName))
			return true;

		final String dir = StringUtil.isNullOrEmpty(path) || ".".equalsIgnoreCase(path) ? ossbuild.Process.getWorkingDirectory() : path;
		if (StringUtil.isNullOrEmpty(dir))
			return true;

		final File d = new File(dir);
		if (!d.exists())
			return false;

		NativeLibrary.addSearchPath(libraryName, path);
		return true;
	}
}
