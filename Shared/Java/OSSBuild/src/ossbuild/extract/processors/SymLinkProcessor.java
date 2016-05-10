
package ossbuild.extract.processors;

import java.io.File;
import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathException;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import ossbuild.Path;
import ossbuild.StringUtil;
import ossbuild.extract.DefaultResourceProcessor;
import ossbuild.extract.IResourceFilter;
import ossbuild.extract.IResourcePackage;
import ossbuild.extract.IResourceProgressListener;
import ossbuild.extract.IVariableProcessor;
import ossbuild.extract.ResourceProcessor;
import ossbuild.extract.ResourceProcessorFactory;

/**
 * Makes a new directory at the provided path.
 * 
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
@ResourceProcessor(
	tagName = "SymLink",
	supportsSize = false
)
public class SymLinkProcessor extends DefaultResourceProcessor {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  ATTRIBUTE_TO      = "to"
		, ATTRIBUTE_FROM    = "from"
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private String to = StringUtil.empty;
	private String from = StringUtil.empty;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public SymLinkProcessor() {
	}

	public SymLinkProcessor(File from, File to) {
		this(from.getAbsolutePath(), to.getAbsolutePath(), StringUtil.empty, StringUtil.empty);
	}

	public SymLinkProcessor(String from, String to) {
		this(from, to, StringUtil.empty, StringUtil.empty);
	}

	public SymLinkProcessor(String from, String to, String Title, String Description) {
		super(false, StringUtil.empty, StringUtil.empty, StringUtil.empty, Title, Description);

		this.to = to;
		this.from = from;
	}
	//</editor-fold>
	
	@Override
	protected boolean loadSettings(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		this.to = stringAttributeValue(varproc, StringUtil.empty, node, ATTRIBUTE_TO);
		this.from = stringAttributeValue(varproc, StringUtil.empty, node, ATTRIBUTE_FROM);
		
		return true;
	}

	@Override
	protected boolean processResource(final String fullResourceName, final IResourcePackage pkg, final IResourceFilter filter, final IResourceProgressListener progress) {
		final String tmpTo = StringUtil.isNullOrEmpty(to) || ".".equalsIgnoreCase(to) ? pkg.getDirectory() : to;
		final String tmpFrom = StringUtil.isNullOrEmpty(from) || ".".equalsIgnoreCase(from) ? pkg.getDirectory() : from;

		if (StringUtil.isNullOrEmpty(tmpFrom) || StringUtil.isNullOrEmpty(tmpTo))
			return true;

		final File fFrom = new File(tmpFrom);
		if (fFrom.exists())
			return true;
		
		final File fTo = new File(tmpTo);

		return Path.createSymbolicLink(fTo, fFrom);
	}
}
