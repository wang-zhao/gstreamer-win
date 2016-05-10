
package ossbuild.extract.processors;

import javax.xml.xpath.XPath;
import javax.xml.xpath.XPathException;
import org.w3c.dom.Document;
import org.w3c.dom.Node;
import ossbuild.StringUtil;
import ossbuild.extract.IResourcePackage;
import ossbuild.extract.IVariableProcessor;
import ossbuild.extract.ResourceProcessor;
import ossbuild.extract.ResourceProcessorFactory;

/**
 * Processes or doesn't process resources depening on whether or not a variable
 * is defined and/or has a certain value.
 * 
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
@ResourceProcessor(
	tagName = "IfNotDef",
	supportsSize = true
)
public class IfNotDefProcessor extends ContainerProcessor {
	//<editor-fold defaultstate="collapsed" desc="Constants">
	public static final String
		  ATTRIBUTE_VAR         = "var"
		, ATTRIBUTE_VALUE       = "value"
		, ATTRIBUTE_IGNORECASE  = "ignoreCase"
	;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Variables">
	private String var = StringUtil.empty;
	private String value = StringUtil.empty;
	private boolean ignoreCase = true;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public IfNotDefProcessor() {
		super();
	}

	public IfNotDefProcessor(String var) {
		this(var, StringUtil.empty, StringUtil.empty, StringUtil.empty);
	}

	public IfNotDefProcessor(String var, String Title, String Description) {
		this(var, StringUtil.empty, Title, Description);
	}

	public IfNotDefProcessor(String var, String value, String Title, String Description) {
		super(Title, Description);

		this.var = var;
		this.value = value;
	}
	//</editor-fold>
	
	@Override
	protected boolean loadContainerSettingsAfter(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		this.var = stringAttributeValue(varproc, StringUtil.empty, node, ATTRIBUTE_VAR);
		this.value = stringAttributeValue(varproc, null, node, ATTRIBUTE_VALUE);
		this.ignoreCase = boolAttributeValue(varproc, true, node, ATTRIBUTE_IGNORECASE);

		//If we don't have a variable to examine or it doesn't exist, then there's
		//nothing to do here.
		if (StringUtil.isNullOrEmpty(var) || !varproc.hasVariable(var))
			return true;


		//If we don't have a value we need to compare against, then assume
		//that we don't want to proceess the inner resources.
		if (value == null)
			return clearProcessors();

		//At this point, we have a value and we have a variable. Let's check if the values are equivalent.

		final String varValue = varproc.findValue(var);
		if (varValue == null) 
			return true;

		if (ignoreCase && !value.equalsIgnoreCase(varValue))
			return true;

		if (!ignoreCase && !value.equals(varValue))
			return true;

		return clearProcessors();
	}
}
