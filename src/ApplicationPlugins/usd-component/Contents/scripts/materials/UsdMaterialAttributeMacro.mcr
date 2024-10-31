macroScript AddUSDCustomAttrToMaterialSelection category:"USD" buttonText:"Add USD Cust Attr to Selected Materials" tooltip:"Add USD Cust Attr to Selected Materials"
(
	usdHolder = USDMaterialAttributeHolder
	if sme.IsOpen() do
	(
		v = sme.GetView sme.activeView
		selectedNode = v.GetSelectedNodes()
		for n = 1 to selectedNode.count do
		(
			m = selectedNode[n].reference
			if (superClassOf m) == material then
			(
				custAttributes.add m usdHolder
			)
		)
	)
)

macroScript AddUSDCustomAttrToObjectSelection category:"USD" buttonText:"Add USD Cust Attr to Selected objects" tooltip:"Add USD Cust Attr to Selected objects"
(
	usdHolder = USDMaterialAttributeHolder
	for n = 1 to selection.count do
	(
		if selection[n].material != undefined do
		(
			custAttributes.add selection[n].material usdHolder
		)
	)
)