
package ossbuild.extract.processors;

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
 * Sets a Java system property.
 * 
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
@ResourceProcessor(
	tagName = "SystemProperty",
	supportsSize = false
)
public class SystemPropertyProcessor extends DefaultResourceProcessor {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  ATTRIBUTE_KEY      = "key"
		, ATTRIBUTE_VALUE    = "value"
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private String key = StringUtil.empty;
	private String value = StringUtil.empty;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public SystemPropertyProcessor() {
	}

	public SystemPropertyProcessor(String message) {
		this(StringUtil.empty, message, StringUtil.empty, StringUtil.empty);
	}

	public SystemPropertyProcessor(String message, String Title, String Description) {
		this(StringUtil.empty, message, Title, Description);
	}

	public SystemPropertyProcessor(String i18nKey, String message, String Title, String Description) {
		super(false, StringUtil.empty, StringUtil.empty, StringUtil.empty, Title, Description);

		this.key = message;
	}
	//</editor-fold>
	
	@Override
	protected boolean loadSettings(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		this.key = stringAttributeValue(varproc, StringUtil.empty, node, ATTRIBUTE_KEY);
		this.value = stringAttributeValue(varproc, StringUtil.empty, node, ATTRIBUTE_VALUE);
		
		return true;
	}

	@Override
	protected boolean processResource(final String fullResourceName, final IResourcePackage pkg, final IResourceFilter filter, final IResourceProgressListener progress) {
		if (!StringUtil.isNullOrEmpty(key))
			System.setProperty(key, value);
		return true;
	}
}
