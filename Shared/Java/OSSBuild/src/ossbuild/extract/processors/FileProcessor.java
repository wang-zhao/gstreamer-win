
package ossbuild.extract.processors;

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
import ossbuild.extract.ResourceUtils;

/**
 *
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
@ResourceProcessor(
	tagName = "File",
	supportsSize = true
)
public class FileProcessor extends DefaultResourceProcessor {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  ATTRIBUTE_EXECUTABLE      = "executable"
		, ATTRIBUTE_IGNORE_MISSING  = "ignoreMissing"
	;
	//</editor-fold>
	
	//<editor-fold defaultstate="collapsed" desc="Variables">
	private boolean executable = false;
	private boolean ignoreMissing = false;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public FileProcessor() {
	}

	public FileProcessor(boolean Executable, String ResourceName) {
		this(Executable, false, ResourceName, StringUtil.empty, StringUtil.empty, StringUtil.empty, StringUtil.empty);
	}

	public FileProcessor(boolean IsTransient, String ResourceName, String SubDirectory, String DestName, String Title, String Description) {
		this(false, IsTransient, ResourceName, SubDirectory, DestName, Title, Description);
	}

	public FileProcessor(boolean Executable, boolean IsTransient, String ResourceName, String SubDirectory, String DestName, String Title, String Description) {
		super(IsTransient, ResourceName, SubDirectory, DestName, Title, Description);
		this.executable = Executable;
	}
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Getters">
	public boolean isExecutable() {
		return executable;
	}
	//</editor-fold>
	
	@Override
	protected boolean loadSettings(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		this.size = ResourceUtils.sizeFromResource(fullResourceName);
		this.executable = boolAttributeValue(varproc, false, node, ATTRIBUTE_EXECUTABLE);
		this.ignoreMissing = boolAttributeValue(varproc, false, node, ATTRIBUTE_IGNORE_MISSING);
		
		return true;
	}

	@Override
	protected boolean processResource(final String fullResourceName, final IResourcePackage pkg, final IResourceFilter filter, final IResourceProgressListener progress) {
		final File dest = pkg.filePath(subDirectory, destName);
		
		if (!ResourceUtils.extractResource(fullResourceName, dest, shouldBeTransient))
			return ignoreMissing;

		if (executable && !dest.setExecutable(true))
			return false;

		return true;
	}
}
