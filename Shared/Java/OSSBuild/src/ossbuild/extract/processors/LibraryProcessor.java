
package ossbuild.extract.processors;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathConstants;
import javax.xml.xpath.XPathException;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import ossbuild.StringUtil;
import ossbuild.extract.DefaultResourceProcessor;
import ossbuild.extract.IResourceFilter;
import ossbuild.extract.IResourcePackage;
import ossbuild.extract.IResourceProgressListener;
import ossbuild.extract.IVariableProcessor;
import ossbuild.extract.ResourceProcessor;
import ossbuild.extract.ResourceProcessorFactory;
import ossbuild.extract.ResourceUtils;

/**
 * Attempts to read in and load a shared library. It can try multiple
 * system libraries first. If none of those work, then it will attempt
 * to extract and load the specified resource shared library.
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
@ResourceProcessor(
	tagName = "Library",
	supportsSize = true
)
public class LibraryProcessor extends DefaultResourceProcessor {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  ATTRIBUTE_EXECUTABLE      = "executable"
		, ATTRIBUTE_IGNORE_MISSING  = "ignoreMissing"
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private List<String> systemAttempts = null;
	private boolean executable = false;
	private boolean ignoreMissing = false;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public LibraryProcessor() {
	}

	public LibraryProcessor(boolean Executable, String ResourceName) {
		this(Executable, false, ResourceName, StringUtil.empty, StringUtil.empty, StringUtil.empty, StringUtil.empty);
	}

	public LibraryProcessor(boolean IsTransient, String ResourceName, String SubDirectory, String DestName, String Title, String Description) {
		this(false, IsTransient, ResourceName, SubDirectory, DestName, Title, Description);
	}

	public LibraryProcessor(boolean Executable, boolean IsTransient, String ResourceName, String SubDirectory, String DestName, String Title, String Description) {
		super(IsTransient, ResourceName, SubDirectory, DestName, Title, Description);
		this.executable = Executable;
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Getters">
	public boolean isExecutable() {
		return executable;
	}
	//</editor-fold>
	
	//<editor-fold defaultstate="collapsed" desc="Public Methods">
	public void addSystemAttempt(String value) {
		if (systemAttempts == null)
			systemAttempts = new ArrayList<String>(1);
		systemAttempts.add(value);
	}

	public void clearSystemAttempts() {
		if (systemAttempts == null)
			return;
		systemAttempts.clear();
	}
	//</editor-fold>

	@Override
	protected boolean loadSettings(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		this.size = ResourceUtils.sizeFromResource(fullResourceName);
		this.executable = boolAttributeValue(varproc, false, node, ATTRIBUTE_EXECUTABLE);
		this.ignoreMissing = boolAttributeValue(varproc, false, node, ATTRIBUTE_IGNORE_MISSING);
		
		NodeList lst;
		Node childNode;

		if ((lst = (NodeList)xpath.evaluate("SystemAttempt", node, XPathConstants.NODESET)) == null || lst.getLength() <= 0)
			return true;

		//Iterate over every <SystemAttempt /> tag, extract its value, and
		//then add it to our list.
		for(int i = 0; i < lst.getLength() && (childNode = lst.item(i)) != null; ++i)
			addSystemAttempt(expandVariables(varproc, childNode.getTextContent().trim()));
		return true;
	}

	@Override
	protected boolean processResource(final String fullResourceName, final IResourcePackage pkg, final IResourceFilter filter, final IResourceProgressListener progress) {
		if (systemAttempts != null && !systemAttempts.isEmpty())
			for(String libAttempt : systemAttempts)
				if (!StringUtil.isNullOrEmpty(libAttempt) && ResourceUtils.attemptSystemLibraryLoad(libAttempt))
					return true;

		final File dest = pkg.filePath(subDirectory, destName);
		if (!ResourceUtils.extractResource(fullResourceName, dest, shouldBeTransient))
			return ignoreMissing;

		if (executable && !dest.setExecutable(true))
			return false;
		
		return ResourceUtils.attemptLibraryLoad(dest.getAbsolutePath());
	}
}
