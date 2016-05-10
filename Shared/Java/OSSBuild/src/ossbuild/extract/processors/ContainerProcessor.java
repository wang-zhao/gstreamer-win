
package ossbuild.extract.processors;

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
import ossbuild.extract.IResourceProcessor;
import ossbuild.extract.IResourceProgressListener;
import ossbuild.extract.IVariableProcessor;
import ossbuild.extract.ResourceProcessorFactory;

/**
 * Processor that holds other processors. Useful for ifdef's, ifnotdef's, etc.
 * 
 * @author David Hoyt <dhoyt@hoytsoft.org>
 */
public abstract class ContainerProcessor extends DefaultResourceProcessor {
	//<editor-fold defaultstate="collapsed" desc="Variables">
	protected List<IResourceProcessor> processors = null;
	//</editor-fold>

	//<editor-fold defaultstate="collapsed" desc="Initialization">
	public ContainerProcessor() {
		super();
	}

	public ContainerProcessor(String Title, String Description) {
		super(false, StringUtil.empty, StringUtil.empty, StringUtil.empty, Title, Description);
	}
	//</editor-fold>

	protected boolean clearProcessors() {
		if (processors == null || processors.isEmpty())
			return true;
		processors.clear();
		return true;
	}
	
	@Override
	public boolean supportsSize() {
		return (processors != null && !processors.isEmpty());
	}

	@Override
	protected long requestSize() {
		if (processors == null || processors.isEmpty())
			return 0L;

		long totalSize = 0L;

		//Calculate total size (in bytes) of all the resources
		for(IResourceProcessor p : processors) {
			if (p == null)
				continue;
			if (p.supportsSize())
				totalSize += p.getSize();
		}

		return totalSize;
	}
	
	@Override
	protected final boolean loadSettings(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		NodeList lst;
		Node childNode;
		IResourceProcessor processor;

		boolean containerSettingsBefore = loadContainerSettingsBefore(fullResourceName, pkg, xpath, node, document, varproc, factory);

		if ((lst = (NodeList)xpath.evaluate("*", node, XPathConstants.NODESET)) == null || lst.getLength() <= 0)
			return loadContainerSettingsAfter(fullResourceName, pkg, xpath, node, document, varproc, factory) && containerSettingsBefore;

		processors = new ArrayList<IResourceProcessor>(3);
		
		//Iterate over every child tag (e.g. <Library />, <File />), extract its name, attempt to
		//create a resource processor, and then add it to our list.
		for(int i = 0; i < lst.getLength() && (childNode = lst.item(i)) != null; ++i) {
			if ((processor = factory.createProcessor(childNode.getNodeName())) != null) {
				if (processor.load(pkg, xpath, childNode, document, varproc, factory))
					processors.add(processor);
			}
		}

		return loadContainerSettingsAfter(fullResourceName, pkg, xpath, node, document, varproc, factory) && containerSettingsBefore;
	}

	@Override
	protected final boolean processResource(final String fullResourceName, final IResourcePackage pkg, final IResourceFilter filter, final IResourceProgressListener progress) {
		if (!processContainerResourceBefore(fullResourceName, pkg, progress))
			return false;

		boolean ret = true;
		if (processors != null && !processors.isEmpty()) {
			String resName;
			for(IResourceProcessor p : processors) {
				//Double check that we're allowed to process this resource
				resName = pkg.resourcePath(p.getName());
				if (filter != null && !filter.filter(pkg, p, resName))
					continue;

				//If this was already processed, then skip the processing again
				ret = (p.isProcessed() || p.process(resName, pkg, filter, progress)) && ret;
			}
		}

		return processContainerResourceAfter(fullResourceName, pkg, progress) && ret;
	}

	protected boolean loadContainerSettingsBefore(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		return true;
	}

	protected boolean loadContainerSettingsAfter(final String fullResourceName, final IResourcePackage pkg, final XPath xpath, final Node node, final Document document, final IVariableProcessor varproc, final ResourceProcessorFactory factory) throws XPathException {
		return true;
	}

	protected boolean processContainerResourceBefore(final String fullResourceName, final IResourcePackage pkg, final IResourceProgressListener progress) {
		return true;
	}

	protected boolean processContainerResourceAfter(final String fullResourceName, final IResourcePackage pkg, final IResourceProgressListener progress) {
		return true;
	}
}
